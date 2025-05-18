
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
        time_t timeStamp;
        char msg[100];
        char level;
    };

    enum class LogLevel
    {
        Error = 0,
        Warning = 1,
        Info = 2,
        Debug = 3,
        Verbose = 4
    };

    SPIFFSLoggerClass();
    ~SPIFFSLoggerClass();
    void Initialize(const String& filename, int max_records);
    void setLogLevel(LogLevel logLevel){mLogLevel = logLevel;}
    void writeLog(LogLevel logLevel,const char *msg, ...);
    void write_next_entry(LogLevel logLevel,const char *msg, ...);
    void read_logs_start(bool reverseMode = false);
    bool read_next_entry(SPIFFSLoggerClass::logEntry& ent);
    void read_logs_end();
    void clearLog();
    void enabled(bool enabled){ locker guard(*this); mEnabled = enabled;}
    bool isEnabled(){ return mEnabled;};
    int numOfLogs() { return mHeader.mNumWrittenLogs; }
    size_t logSize(){ return mHeader.mRealLogsSize; }
    uint32_t numOfLogsPerSession() { return mPerSessionLogs; }

private:
    struct Header {
        int mMaxLogsNumber;  // controls maximum size of file
        int mLogStartIndex;
        int mNumWrittenLogs;  // controls maximum size of file
        int mRealLogsSize;
    };
    bool read_entry (int recnum, SPIFFSLoggerClass::logEntry& ent);
    void write_next_entry(LogLevel logLevel, const char *msg, va_list args);
    bool readHeader();
    bool writeHeader();
    void initLog(int maxlog);
    int mNextWriteLogIndex;  // next record number to write
    Header mHeader;
    int mNextLogToReadIdx;
    int mNumOfreadLogs;
    File mLogFile;
    String mFileName;
    uint32_t mPerSessionLogs;
    LogLevel mLogLevel;
    bool mEnabled;
    bool mReadLogReverseMode;
    static const size_t cHeaderSize = sizeof(Header);
    static const size_t cRecordSize = sizeof(SPIFFSLoggerClass::logEntry);
};

extern SPIFFSLoggerClass SPIFFSLogger;
#define _RF_(x) String(F(x)).c_str()
#define LOG_TO_FILE_E(x,...) SPIFFSLogger.writeLog(SPIFFSLoggerClass::LogLevel::Error,(x),##__VA_ARGS__)
#define LOG_TO_FILE_W(x,...) SPIFFSLogger.writeLog(SPIFFSLoggerClass::LogLevel::Warning,(x),##__VA_ARGS__)
#define LOG_TO_FILE_I(x,...) SPIFFSLogger.writeLog(SPIFFSLoggerClass::LogLevel::Info,(x),##__VA_ARGS__)
#define LOG_TO_FILE_D(x,...) SPIFFSLogger.writeLog(SPIFFSLoggerClass::LogLevel::Debug,(x),##__VA_ARGS__)
#define LOG_TO_FILE_V(x,...) SPIFFSLogger.writeLog(SPIFFSLoggerClass::LogLevel::Verbose,(x),##__VA_ARGS__)
#define LOG_TO_FILE(x,...)   SPIFFSLogger.write_next_entry(SPIFFSLoggerClass::LogLevel::Verbose,_RF_(x),##__VA_ARGS__)
#else
#define LOG_TO_FILE(x,...)
#define LOG_TO_FILE_E(x,...)
#define LOG_TO_FILE_W(x,...)
#define LOG_TO_FILE_I(x,...)
#define LOG_TO_FILE_D(x,...)
#define LOG_TO_FILE_V(x,...)
#endif /*ENABLE_FILE_LOG*/
#endif /*SPIFFSLOGGER_H*/
