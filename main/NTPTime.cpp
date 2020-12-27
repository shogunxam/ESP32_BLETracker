#include <WiFi.h>
#include <time.h>
#include "config.h"

namespace NTPTime
{
    static bool NPTIMEInitializing = false;
    void initialize()
    {
        NPTIMEInitializing = true;
        ::configTime(GMT_OFFSET_IN_SEC, DST_OFFSET_INSEC, NTP_SERVER);
        delay(100); //Wait for NTP server response
        NPTIMEInitializing = false;
    }

    void getLocalTime(struct tm &timeinfo)
    {
        while (NPTIMEInitializing)
            delay(10);
        ::getLocalTime(&timeinfo);
    }

    void strftime(const String &format, const tm &timeInfo, String &out)
    {
        char buff[255];
        ::strftime(buff, 255, format.c_str(), &timeInfo);
        out = buff;
    }

    void strftime(const String &format, const tm &timeInfo, char *outBuff, int size)
    {
        ::strftime(outBuff, size, format.c_str(), &timeInfo);
    }
} // namespace NTPTime