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
        void SendChunkedContent(const uint8_t *content, size_t size);
        void SendChunkedContent(const char *content);
        void FlushChunkedContent();

        size_t append(uint8_t *dest, size_t buffsize, size_t destStartPos, const uint8_t *src, size_t srcSize, size_t srcStartPos = 0);
        size_t appendAndFlush(uint8_t *dest, size_t buffsize, size_t destStartPos, const uint8_t *src, size_t srcSize);
        WebServer server;
        String hostName;
        String ssid;
        String password;
        bool serverRunning;
        MyMutex dataBuffMutex;
};
#endif /*OTA_WEB_SERVER*/