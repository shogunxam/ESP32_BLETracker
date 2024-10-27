#include <WiFi.h>
#include <time.h>
#include <map>
#include <cstring>
#include "config.h"
#include "DebugPrint.h"

// TimeZone management
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ezTime.h>
/////

namespace NTPTime
{
    struct cmpStr
    {
        bool operator()(char const *a, char const *b) const
        {
            return std::strcmp(a, b) < 0;
        }
    };

    static bool NPTIMEInitializing = false;
    void initialize(const char* timeZone)
    {
        const char* Tz = "GMT0";
        if(timeZone != nullptr)
        {
            Tz = timeZone;
        }
        DEBUG_PRINTF("TZ is %s\n", Tz);
        NPTIMEInitializing = true;
        ::configTzTime(Tz, NTP_SERVER);
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

    String GetTimezoneFromWeb() 
    {
        int retry = 3;
        String Tz = "GMT0";
        HTTPClient http;
        int statusCode = 0;
        do
        {        
            http.begin("http://ip-api.com/json");
            statusCode = http.GET();
            DEBUG_PRINTF("HttP code %d\n", statusCode);
            
            if (statusCode == HTTP_CODE_OK)
            {
                StaticJsonDocument<512> doc;
                deserializeJson(doc, http.getString());
                Tz = doc["timezone"].as<String>();
                DEBUG_PRINTF("HttP timezone %s\n", Tz.c_str());
            }
            http.end();
        }while(retry-- > 0 && statusCode!= HTTP_CODE_OK);
        
        Timezone myTZ;
        myTZ.setLocation(Tz);
        return   myTZ.getPosix(); 
    }
} // namespace NTPTime