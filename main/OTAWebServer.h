#ifndef OTA_WEB_SERVER
#define OTA_WEB_SERVER
#include <WebServer.h>
#include "myMutex.h"

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
        void getIndexData();
        void getOTAUpdate();
        void getConfig();
        void postUpdateConfig();
        void getServerInfo();
        #if ENABLE_FILE_LOG
        void eraseLogs();
        void getLogs();
        void getLogsData();
        #endif
        void StartChunkedContentTransfer(const String& contentType);
        void InitChunkedContent();
        void SendChunkedContent(const char* content);
        void FlushChunkedContent();
        void SendContent(const String& content);
        void SendContent_P(PGM_P content);

        size_t concat(char* dest, size_t buffsize, const char* src, size_t startpos=0);
        void concatAndFlush(char* dest, size_t buffsize, const char* src);
        WebServer server;
        String hostName;
        String ssid;
        String password;
        bool serverRunning;
        MyMutex dataBuffMutex;
};
#endif /*OTA_WEB_SERVER*/