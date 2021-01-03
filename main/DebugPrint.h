#include <mutex>
#include "config.h"

extern char _printbuffer_[256];
extern std::mutex _printLock_;
#if defined(DEBUG_SERIAL)
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(x,...) {\
std::lock_guard<std::mutex> lock(_printLock_); \
snprintf(_printbuffer_,255,x,__VA_ARGS__);\
Serial.print(_printbuffer_);\
}
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(x,...) 
#endif
#define SERIAL_PRINTF(x,...) {\
std::lock_guard<std::mutex> lock(_printLock_); \
snprintf(_printbuffer_,255,x,__VA_ARGS__);\
Serial.print(_printbuffer_);\
}