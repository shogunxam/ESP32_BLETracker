#include <mutex>
#include "config.h"
#include "NTPTime.h"
extern char _printbuffer_[256];
extern std::mutex _printLock_;
#if defined(DEBUG_SERIAL)
#define DEBUG_PRINTTIME()                                                         \
    {                                                                             \
        struct tm timeInfo;                                                       \
        char timstp[21];                                                          \
        NTPTime::getLocalTime(timeInfo);                                          \
        NTPTime::strftime("%Y-%m-%d %H:%M:%S ", timeInfo, timstp, sizeof(timstp)); \
        Serial.print(timstp);                                                     \
    }

#define DEBUG_PRINT(x)                                 \
    {                                                  \
        std::lock_guard<std::mutex> lock(_printLock_); \
        DEBUG_PRINTTIME();                             \
        Serial.print(x);                               \
    }
#define DEBUG_PRINTLN(x)                               \
    {                                                  \
        std::lock_guard<std::mutex> lock(_printLock_); \
        DEBUG_PRINTTIME();                             \
        Serial.println(x);                             \
    }
#define DEBUG_PRINTF(x, ...)                                            \
    {                                                                   \
        std::lock_guard<std::mutex> lock(_printLock_);                  \
        DEBUG_PRINTTIME();                                              \
        snprintf(_printbuffer_, sizeof(_printbuffer_), x, __VA_ARGS__); \
        Serial.print(_printbuffer_);                                    \
    }
#else
#define DEBUG_PRINTTIME()
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(x, ...)
#endif
#define SERIAL_PRINTF(x, ...)                                           \
    {                                                                   \
        std::lock_guard<std::mutex> lock(_printLock_);                  \
        snprintf(_printbuffer_, sizeof(_printbuffer_), x, __VA_ARGS__); \
        Serial.print(_printbuffer_);                                    \
    }