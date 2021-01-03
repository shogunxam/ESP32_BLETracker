#ifndef __NTPTIME_h__
#define __NTPTIME_h__
#include <time.h>
#include <WString.h>
namespace NTPTime
{
    void initialize();
    void getLocalTime(tm& timeinfo);
    void strftime(const char* format, const tm& timeInfo, String &out);
    void strftime(const char* format, const tm& timeInfo, char*outBuff, int size);
}
#endif /*__NTPTIME_h__*/