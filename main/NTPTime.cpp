#include <WiFi.h>
#include <time.h>
#include <map>
#include <cstring>
#include "config.h"
#include "DebugPrint.h"

namespace NTPTime
{
    struct cmpStr
    {
        bool operator()(char const *a, char const *b) const
        {
            return std::strcmp(a, b) < 0;
        }
    };

    static std::map<const char*, const char*, cmpStr> TimeZones = {
        {"Alaska Daylight Time","AKST9AKDT,M3.2.0,M11.1.0"},
        {"Arabia Daylight Time","AST4ADT,M3.2.0,M11.1.0"},
        {"Atlantic Standard Time","AST4"},
        {"Australian Central Daylight Time","ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
        {"Australian Central Standard Time","ACST-9:30"},
        {"Australian Eastern Daylight Time","AEST-10AEDT,M10.1.0,M4.1.0/3"},
        {"Australian Eastern Standard Time","AEST-10"},
        {"Australian Western Standard Time","AWST-8"},
        {"British Summer Time","GMT0BST,M3.5.0/1,M10.5.0"},
        {"Central Africa Time","CAT-2"},
        {"Central Daylight Time","CST6CDT,M3.2.0,M11.1.0"},
        {"Central European Summer Time","CET-1CEST,M3.5.0,M10.5.0/3"},
        {"Central European Time","CET-1"},
        {"Central Standard Time","CST6"},
        {"Chamorro Standard Time","ChST-10"},
        {"China Standard Time","CST-8"},
        {"Cuba Standard Time","CST5CDT,M3.2.0/0,M11.1.0/1"},
        {"Eastern Africa Time","EAT-3"},
        {"Eastern Daylight Time","EST5EDT,M3.2.0,M11.1.0"},
        {"Eastern European Summer Time","EET-2EEST,M3.5.0/3,M10.5.0/4"},
        {"Eastern European Time","EET-2"},
        {"Eastern Standard Time","EST5"},
        {"Greenwich Mean Time","GMT0"},
        {"Hawaii Standard Time","HST10"},
        {"Hawaii-Aleutian Daylight Time","HST10HDT,M3.2.0,M11.1.0"},
        {"Hong Kong Time","HKT-8"},
        {"Irish Standard Time","IST-1GMT0,M10.5.0,M3.5.0/1"},
        {"Israel Standard Time","IST-2IDT,M3.4.4/26,M10.5.0"},
        {"Japan Standard Time","JST-9"},
        {"Korea Standard Time","KST-9"},
        {"Moscow Standard Time","MSK-3"},
        {"Mountain Daylight Time","MST7MDT,M3.2.0,M11.1.0"},
        {"Mountain Standard Time","MST7"},
        {"New Zealand Daylight Time","NZST-12NZDT,M9.5.0,M4.1.0/3"},
        {"Pacific Daylight Time","PST8PDT,M3.2.0,M11.1.0"},
        {"Samoa Standard Time","SST11"},
        {"South Africa Standard Time","SAST-2"},
        {"West Africa Time","WAT-1"},
        {"Western European Summer Time","WET0WEST,M3.5.0/1,M10.5.0"},
        {"Western Indonesian Time","WIB-7"}
    };

    static bool NPTIMEInitializing = false;
    void initialize(const char* timeZone)
    {
        auto tz = TimeZones.find(timeZone);
        const char* posixTz = "GMT0";
        if(tz != TimeZones.end())
        {
            posixTz = tz->second;
        }
        else
        {
            posixTz = timeZone;
        }
        DEBUG_PRINTF("TZ %s",posixTz);
        NPTIMEInitializing = true;
        ::configTzTime(posixTz, NTP_SERVER);
        delay(500); //Wait for NTP server response
        NPTIMEInitializing = false;
    }

    void getLocalTime(struct tm &timeinfo)
    {
        while (NPTIMEInitializing)
            delay(10);
        ::getLocalTime(&timeinfo);
    }

    time_t getTimeStamp()
    {
        struct tm timeinfo;
        getLocalTime(timeinfo);
        return mktime(&timeinfo);
    }
    
    void strftime(const char* format, const tm &timeInfo, String &out)
    {
        char buff[255];
        ::strftime(buff, 255, format, &timeInfo);
        out = buff;
    }

    void strftime(const char* format, const tm &timeInfo, char *outBuff, int size)
    {
        ::strftime(outBuff, size, format, &timeInfo);
    }

    unsigned long seconds()
    {
        return millis() / 1000;
    }
} // namespace NTPTime