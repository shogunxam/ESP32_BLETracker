#include "SPIFFSLogger.h"
#if ENABLE_FILE_LOG
#include <FS.h>
#include "DebugPrint.h"

#define ADVANCE_CIRCULAR_INDEX(x) x = (x + 1) % mHeader.mMaxLogsNumber
#define BACK_CIRCULAR_INDEX(x) x = x <= 0 ? (mHeader.mNumWrittenLogs - 1) : (x - 1)
#define RECORD_POSITION(x) (cHeaderSize + (x * cRecordSize))

SPIFFSLoggerClass::SPIFFSLoggerClass() : MyMutex("SPIFFSLogger"), mLogLevel(LogLevel::Info) {}

void SPIFFSLoggerClass::Initialize(const String &fileName, int max_records)
{
    mPerSessionLogs = 0;
    mEnabled = true;
    mReadLogReverseMode = false;
    mFileName = fileName;

    mLogFile = SPIFFS.open(mFileName, "r+");
    if (mLogFile)
    {
        readHeader();
        mLogFile.close();
        if (mHeader.mMaxLogsNumber != max_records || (mHeader.mLogStartIndex == -1 && mHeader.mNumWrittenLogs == 0))
        {
            // Clear the existing file
            mHeader.mMaxLogsNumber = max_records;
            clearLog();
        }
    }
    else
    {
        if (SPIFFS.exists(fileName))
        {
            DEBUG_PRINTF("Filed to open %s\n", fileName.c_str());
        }
        else
        {
            DEBUG_PRINTF("File %s not exists\n", fileName.c_str());
        }

        // Clear the existing file
        mHeader.mMaxLogsNumber = max_records;
        clearLog();
    }

    mNextWriteLogIndex = (mHeader.mLogStartIndex + mHeader.mNumWrittenLogs) % mHeader.mMaxLogsNumber;
    if (mNextWriteLogIndex < 0)
        mNextWriteLogIndex = 0;

    DEBUG_PRINTF("SPIFFSLogger  first_record %d, num_written_record %d,  maxLogsNum %d\n", mHeader.mLogStartIndex, mHeader.mNumWrittenLogs, mHeader.mMaxLogsNumber);
}

void SPIFFSLoggerClass::clearLog()
{
    locker guard(*this);
    SPIFFS.remove(mFileName);
    mLogFile = SPIFFS.open(mFileName, "w");
    initLog(mHeader.mMaxLogsNumber);
    mLogFile.close();
    mNextWriteLogIndex = 0;
}

void SPIFFSLoggerClass::initLog(int maxlog)
{
    mHeader.mLogStartIndex = -1;
    mHeader.mNumWrittenLogs = 0;
    mHeader.mMaxLogsNumber = maxlog;
    mHeader.mRealLogsSize = 0;
    writeHeader();
}

SPIFFSLoggerClass::~SPIFFSLoggerClass()
{
    mLogFile.close();
}

bool SPIFFSLoggerClass::readHeader()
{
    if (!mLogFile)
        return false;

    size_t bytes = 0;
    if (mLogFile.seek(0, SeekMode::SeekSet))
        bytes = mLogFile.read((uint8_t *)&mHeader, cHeaderSize);
    return bytes == cHeaderSize;
}

bool SPIFFSLoggerClass::writeHeader()
{
    if (!mLogFile)
        return false;
    size_t bytes = 0;
    if (mLogFile.seek(0, SeekMode::SeekSet))
    {
        bytes = mLogFile.write((uint8_t *)&mHeader, cHeaderSize);
        mLogFile.flush();
    }

    return bytes == cHeaderSize;
}
void SPIFFSLoggerClass::writeLog(LogLevel logLevel, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    write_next_entry(logLevel, msg, args);
    va_end(args);
}

void SPIFFSLoggerClass::write_next_entry(LogLevel logLevel, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    write_next_entry(logLevel, msg, args);
    va_end(args);
}

void SPIFFSLoggerClass::write_next_entry(LogLevel logLevel, const char *msg, va_list args)
{
    if (logLevel > mLogLevel)
        return;

    locker guard(*this);

    if (!mEnabled)
        return;

    mLogFile = SPIFFS.open(mFileName, "r+");
    if (!mLogFile)
        return;

    logEntry ent;

    memset(&ent, 0x0, sizeof(ent));

    // format message into entry
    // The generated string has a length of at sizeof(ent.msg)-1, leaving space for the additional terminating null character.
    vsnprintf(ent.msg, sizeof(ent.msg), msg, args);

    // struct tm timeInfo;
    // NTPTime::getLocalTime(timeInfo);
    // NTPTime::strftime("%Y-%m-%d %H:%M:%S", timeInfo, ent.timeStamp, sizeof(ent.timeStamp));
    ent.timeStamp = NTPTime::getTimeStamp();

    size_t bytes = 0;
    log_i("Writing to record %d T:%s, M:%s", mNextWriteLogIndex, ent.timeStamp, ent.msg);
    ent.level = char(logLevel);

    int overwrittenElemSize = 0;
    if (mHeader.mNumWrittenLogs == mHeader.mMaxLogsNumber)
    {
        logEntry tmpent;
        mLogFile.seek(RECORD_POSITION(mNextWriteLogIndex));
        mLogFile.read((uint8_t *)(&tmpent), cRecordSize);
        overwrittenElemSize = strlen(tmpent.msg) + sizeof(tmpent.timeStamp) + 1;
    }

    if (mLogFile.seek(RECORD_POSITION(mNextWriteLogIndex)))
    {
        bytes = mLogFile.write((uint8_t *)(&ent), cRecordSize);
        mLogFile.flush();
    }
    else
    {
        log_i("FSeek failed file size %d", mLogFile.size());
    }

    if (bytes != cRecordSize)
    {
        log_i("Failed to write log on file");
        mLogFile.close();
        return;
    }

    if (mHeader.mNumWrittenLogs < mHeader.mMaxLogsNumber)
        mHeader.mNumWrittenLogs++;
    int bck = mHeader.mRealLogsSize;
    mHeader.mRealLogsSize += strlen(ent.msg) + sizeof(ent.timeStamp) + 1 - overwrittenElemSize;

    if (!writeHeader())
    {
        mHeader.mRealLogsSize = bck;
        log_i("Failed to write header on file");
        mLogFile.close();
        return;
    }

    mLogFile.close();

    if ((mHeader.mLogStartIndex == mNextWriteLogIndex) || (mHeader.mLogStartIndex < 0))
        ADVANCE_CIRCULAR_INDEX(mHeader.mLogStartIndex);

    ADVANCE_CIRCULAR_INDEX(mNextWriteLogIndex);

    ++mPerSessionLogs;
}

bool SPIFFSLoggerClass::read_entry(int recnum, SPIFFSLoggerClass::logEntry &ent)
{
    if (recnum >= mHeader.mMaxLogsNumber || !mLogFile)
        return false;

    memset(&ent, 0x0, cRecordSize);
    size_t bytesRead = 0;
    log_i("reading record %d", recnum);
    if (mLogFile.seek(RECORD_POSITION(recnum)))
    {
        bytesRead = mLogFile.read((uint8_t *)&ent, cRecordSize);
        if (bytesRead != cRecordSize)
            log_i("Read size %d vs %d", bytesRead, cRecordSize);
    }
    else
    {
        log_i("FSeek failed file size %d", mLogFile.size());
    }

    return (bytesRead == cRecordSize);
}

void SPIFFSLoggerClass::read_logs_start(bool reverseMode)
{
    if (!mEnabled)
        return;
    mReadLogReverseMode = reverseMode;
    mNextLogToReadIdx = mHeader.mLogStartIndex;
    if (mReadLogReverseMode)
        BACK_CIRCULAR_INDEX(mNextLogToReadIdx);

    mNumOfreadLogs = 0;
    log_i("SPIFFSLogger  first_record %d, num_written_record %d", mHeader.mLogStartIndex, mHeader.mNumWrittenLogs);
    mLogFile = SPIFFS.open(mFileName, "r");
}

void SPIFFSLoggerClass::read_logs_end()
{
    if (!mEnabled)
        return;
    if (mLogFile)
        mLogFile.close();
}

bool SPIFFSLoggerClass::read_next_entry(SPIFFSLoggerClass::logEntry &ent)
{
    if ((mNumOfreadLogs >= mHeader.mNumWrittenLogs) || !mEnabled)
        return false;

    if (!read_entry(mNextLogToReadIdx, ent))
    {
        log_i("Failed to read record from file");
        ent.timeStamp = 0;
        strcpy(ent.msg, "Failed to read log from file");
    }
    mNumOfreadLogs++;
    if (mReadLogReverseMode)
        BACK_CIRCULAR_INDEX(mNextLogToReadIdx);
    else
        ADVANCE_CIRCULAR_INDEX(mNextLogToReadIdx);
    return true;
}

SPIFFSLoggerClass SPIFFSLogger;
#endif /*ENABLE_FILE_LOG*/