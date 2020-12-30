#ifndef OTA_WEB_SERVER
#define OTA_WEB_SERVER
#include <WebServer.h>
#include "myMutex.h"

class OTAWebServer
{
    public:
    OTAWebServer();
    void setup(const String& hostName, const String& ssid, const String& password);
    void begin(void);
    private:
        void resetESP32Page();
        void getStyle();

        void getSysInfoData();
        void getIndex();
        void getIndexJs();
        void getIndexData();
        void getOTAUpdate();
        void getOTAUpdateJs();
        void getConfig();
        void getConfigJs();
        void getConfigData();
        void postUpdateConfig();
        void getSysInfo();
        void getSysInfoJs();
        #if ENABLE_FILE_LOG
        void eraseLogs();
        void getLogs();
        void getLogsJs();
        void getLogsData();
        #endif
        void StartChunkedContentTransfer(const String& contentType, bool zipped = false);
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