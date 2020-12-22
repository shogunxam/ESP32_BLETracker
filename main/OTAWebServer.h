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
        void resetESP32Page();
        void getConfigData();
        void getServerInfoData();
        void getIndex();
        void getOTAUpdate();
        void getConfig();
        void postUpdateConfig();
        void getServerInfo();
        void StartContentTransfer(const String& contentType);
        void SendContent(const String& content);
        void SendContent_P(PGM_P content);
        WebServer server;
        String hostName;
        String ssid;
        String password;
        bool serverRunning;
};
#endif /*OTA_WEB_SERVER*/