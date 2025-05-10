#ifndef DEBUG_PRINT_H
#define DEBUG_PRINT_H 
#include "config.h"
#include "NTPTime.h"
#include "myMutex.h"

extern char _printbuffer_[256];
extern MyMutex _printMutex_;

#if defined(DEBUG_SERIAL)
static inline void DEBUG_TIME(char timstp[21])
{
    struct tm timeInfo;
    NTPTime::getLocalTime(timeInfo);
    NTPTime::strftime("%Y-%m-%d %H:%M:%S ", timeInfo, timstp, 21);
}

#define DEBUG_PRINT(x)             \
    {                              \
        char timstp[21];           \
        DEBUG_TIME(timstp);        \
        log_printf("%s %s", timstp, x); \
    }

#define DEBUG_PRINTLN(x)              \
    {                                 \
        char timstp[21];              \
        DEBUG_TIME(timstp);           \
        log_printf("%s %s \n", timstp, x); \
    }

#define DEBUG_PRINTF(x, ...)     \
    {                            \
        char timstp[21];         \
        DEBUG_TIME(timstp);      \
        String s; s.reserve(21 + strlen(x) + 1); \
        s= s + String(timstp) + (" ")+ x; \
        log_printf(s.c_str(), ##__VA_ARGS__); \
    }

#else
#define DEBUG_PRINTTIME()
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(x, ...)
#endif
#endif /* DEBUG_PRINT_H */