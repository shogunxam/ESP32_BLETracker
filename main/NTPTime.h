#ifndef __NTPTIME_h__
#define __NTPTIME_h__
#include <time.h>
#include <WString.h>
namespace NTPTime
{
    void initialize(const char* timezone);
    String GetTimezoneFromWeb();
    void getLocalTime(tm& timeinfo);
    time_t getTimeStamp();
    void strftime(const char* format, const tm& timeInfo, String &out);
    void strftime(const char* format, const tm& timeInfo, char*outBuff, int size);
    unsigned long seconds();
}
#endif /*__NTPTIME_h__*/