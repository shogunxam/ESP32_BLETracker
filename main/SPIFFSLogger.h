
#ifndef SPIFFSLOGGER_H
#define SPIFFSLOGGER_H
#include "config.h"
#if ENABLE_FILE_LOG
#include <SPIFFS.h>
#include <FS.h>
#include <mutex>
#include "NTPTime.h"
#include "myMutex.h"

class SPIFFSLoggerClass: public MyMutex
{
public:
    struct logEntry
    {
        char timeStamp[20];
        char msg[100];
    };

    SPIFFSLoggerClass();
    ~SPIFFSLoggerClass();
    void Initialize(const String& filename, int max_records);
    void write_next_entry(const char *msg, ...);
    void read_logs_start(bool reverseMode = false);
    bool read_next_entry(SPIFFSLoggerClass::logEntry& ent);
    void read_logs_end();
    void clearLog();
    void enabled(bool enabled){ mEnabled = enabled;};
    bool isEnabled(){ return mEnabled;};
    int numOfLogs() { return mHeader.mNumWrittenLogs; }
    size_t logSize(){ return mHeader.mRealLogsSize; }

private:
    struct Header {
        int mMaxLogsNumber;  // controls maximum size of file
        int mLogStartIndex;
        int mNumWrittenLogs;  // controls maximum size of file
        int mRealLogsSize;
    };
    bool read_entry (int recnum, SPIFFSLoggerClass::logEntry& ent);
    bool readHeader();
    bool writeHeader();
    bool read_log_in_memory();
    void initLog(int maxlog);
    int mNextWriteLogIndex;  // next record number to write
    Header mHeader;
    int mNextLogToReadIdx;
    int mNumOfreadLogs;
    File mLogFile;
    String mFileName;

    bool mEnabled;
    bool mReadLogReverseMode;
    static const size_t cHeaderSize = sizeof(Header);
    static const size_t cRecordSize = sizeof(SPIFFSLoggerClass::logEntry);
};

extern SPIFFSLoggerClass SPIFFSLogger;
#define _RF_(x) String(F(x)).c_str()
#define FILE_LOG_WRITE(x,...) SPIFFSLogger.write_next_entry(_RF_(x),##__VA_ARGS__)
#else
#define FILE_LOG_WRITE(x,...)
#endif /*ENABLE_FILE_LOG*/
#endif /*SPIFFSLOGGER_H*/
