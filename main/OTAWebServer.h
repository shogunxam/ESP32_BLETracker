#ifndef OTA_WEB_SERVER
#define OTA_WEB_SERVER
#include <WebServer.h>

class OTAWebServer
{
    public:
    OTAWebServer();
    void setup(const String& hostName, const String& ssid, const String& password);
    void loop(void);
    private:
        WebServer server;
        String hostName;
        String ssid;
        String password;
        bool serverRunning;
};
#endif /*OTA_WEB_SERVER*/