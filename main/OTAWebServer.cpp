#include "main.h"
#include "config.h"

#if ENABLE_OTA_WEBSERVER

#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <string>

#include "WiFiManager.h"
#include "DebugPrint.h"
#include "OTAWebServer.h"
#include "macro_utility.h"
#include "settings.h"

#if ENABLE_FILE_LOG
#include "SPIFFSLogger.h"
#endif

extern std::vector<BLETrackedDevice> BLETrackedDevices;

const char jquery[] PROGMEM = R"=====(<script src="https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js"></script>)=====";

//Import the style.css from file
const char style[] = "<style>"
#include "html/style-min.css.h"
                     "</style>";

/* Server Index Page */
const char otaUpdateHtml[] PROGMEM =
#include "html/otaupdate-min.html.h"
    ;

//Import the config.js script from file
const char otaUpdateJs[] PROGMEM = "<script>"
#include "html/otaupdate-min.js.h"
                                   "</script>";

//Import the config.html page from file
const char configHtml[] PROGMEM =
#include "html/config-min.html.h"
    ;

//Import the config.js script from file
const char configJs[] PROGMEM = "<script>"
#include "html/config-min.js.h"
                                "</script>";

const char indexHtml[] PROGMEM =
#include "html/index-min.html.h"
    ;

const char indexJs[] PROGMEM = "<script>"
#include "html/index-min.js.h"
                                "</script>";

const char sysInfoHtml[] PROGMEM =
#include "html/sysinfo-min.html.h"
    ;

const char sysInfoJs[] PROGMEM = "<script>"
#include "html/sysinfo-min.js.h"
                                 "</script>";

#if ENABLE_FILE_LOG
const char logsHtml[] PROGMEM =
#include "html/logs-min.html.h"
    ;

const char logsJs[] PROGMEM = "<script>"
#include "html/logs-min.js.h"
                              "</script>";
#endif

OTAWebServer::OTAWebServer()
    : server(80), serverRunning(false)
{
}

void OTAWebServer::StartChunkedContentTransfer(const String &contentType)
{
  server.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
  server.sendHeader(F("Pragma"), F("no-cache"));
  server.sendHeader(F("Expires"), F("-1"));
  server.sendHeader(F("Transfer-Encoding"),F("chunked"));
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, contentType, "");
}


//Return the number of characters written
size_t OTAWebServer::concat(char* dest, size_t buffsize, const char* src, size_t startpos)
{
  size_t destLen = strlen(dest);
  size_t available = buffsize - destLen - 1;
  size_t wrote=0;
  char* dstWlkr = dest+destLen;
  const char* srcWlkr = src + startpos;
  while( srcWlkr[wrote] != '\0' && wrote < available)
  {
    *dstWlkr=srcWlkr[wrote];
    wrote++;
    dstWlkr++;
  }
  *dstWlkr='\0';
  return wrote;
}

void OTAWebServer::concatAndFlush(char* dest, size_t buffsize, const char* src)
{
    size_t wrote = concat(dest, buffsize, src);
    while(wrote < strlen(src))
    {
      server.sendContent_P(dest);
      dest[0]='\0';
      wrote+=concat(dest, buffsize, src, wrote);
    }
}

const size_t maxdatasize = 5*1024;
static char databuffer[maxdatasize];

void OTAWebServer::SendContent(const String &content)
{
  server.sendContent(content);
}

void OTAWebServer::SendContent_P(PGM_P content)
{
  server.sendContent_P(content);
}

void OTAWebServer::resetESP32Page()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }

  server.sendHeader("Connection", "close");
  server.send(200, "text/html", F("<div align='center'>Resetting...<br><progress id='g' class='y' value='0' max='100' style='align-self:center; text-align:center;'/></div>"
                "<script>window.onload=function(){};var progval=0;var myVar=setInterval(Prog,80);"
                "function Prog(){progval++;document.getElementById('g').value=progval;"
                "if (progval==100){clearInterval(myVar);setTimeout(Check, 3000);}}"
                "function Check(){if (progval==100){clearInterval(myVar);var ftimeout=setTimeout(null,5000);"
                "window.location='/';}}</script>"));
  ESP.restart();
}

#if ENABLE_FILE_LOG
void OTAWebServer::eraseLogs()
{
  SPIFFSLogger.clearLog();
  server.sendHeader("Connection", "close");
  server.send(200, "text/html", "Ok");
}

void OTAWebServer::getLogsData()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }

  CRITICALSECTION_START(SPIFFSLogger)
  StartChunkedContentTransfer(F("text/json"));
  SPIFFSLoggerClass::logEntry entry;
  SPIFFSLogger.read_logs_start(true);
  bool first = true;
  int count = 0;
  CRITICALSECTION_START(dataBuffMutex)
  InitChunkedContent();
  SendChunkedContent("[");

  while (SPIFFSLogger.read_next_entry(entry))
  {
    if (first)
      first = false;
    else
      SendChunkedContent(",");
    SendChunkedContent( R"({"t":")");
    SendChunkedContent( entry.timeStamp);
    SendChunkedContent( R"(","m":")");
    SendChunkedContent( entry.msg);
    SendChunkedContent( R"("})");
  }

  SendChunkedContent( "]");
  FlushChunkedContent();

  CRITICALSECTION_END //dataBuffMutex
  CRITICALSECTION_END //SPIFFSLogger

}

void OTAWebServer::getLogs()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  CRITICALSECTION_START(dataBuffMutex)
  StartChunkedContentTransfer(F("text/html"));
  InitChunkedContent();
  SendChunkedContent(jquery);
  SendChunkedContent(logsJs);
  SendChunkedContent(style);
  SendChunkedContent(logsHtml);
  FlushChunkedContent();
  CRITICALSECTION_END
}

#endif /*ENABLE_FILE_LOG*/

void OTAWebServer::getConfigData()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  server.sendHeader(F("Connection"), F("close"));
  DEBUG_PRINTLN(SettingsMngr.toJSON());
  if (server.args() > 0 && server.hasArg("factory") && server.arg("factory") == "true")
  {
    Settings factoryValues;
    server.send(200, F("text/json"), factoryValues.toJSON());
    return;
  }

  server.send(200, F("text/json"), SettingsMngr.toJSON());
}

void OTAWebServer::getIndex()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
   CRITICALSECTION_START(dataBuffMutex)
  StartChunkedContentTransfer(F("text/html"));
  InitChunkedContent();
  SendChunkedContent(jquery);
  SendChunkedContent(jquery);
  SendChunkedContent(indexJs);
  SendChunkedContent(style);
  SendChunkedContent(indexHtml);
  FlushChunkedContent();
   CRITICALSECTION_END
}

void OTAWebServer::getIndexData()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  CRITICALSECTION_START(dataBuffMutex)
  StartChunkedContentTransfer("text/json");
  InitChunkedContent();
  SendChunkedContent(R"({"gateway":")" GATEWAY_NAME R"(","logs":)");
  #if ENABLE_FILE_LOG
     SendChunkedContent("true");
  #else
     SendChunkedContent( "false");
  #endif
  SendChunkedContent("}");
  FlushChunkedContent();
  CRITICALSECTION_END
}

void OTAWebServer::getOTAUpdate()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  CRITICALSECTION_START(dataBuffMutex)
  StartChunkedContentTransfer("text/html");
  InitChunkedContent();
  SendChunkedContent(jquery);
  SendChunkedContent(otaUpdateJs);
  SendChunkedContent(style);
  SendChunkedContent(otaUpdateHtml);
  FlushChunkedContent();
  CRITICALSECTION_END
}

void OTAWebServer::getConfig()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  CRITICALSECTION_START(dataBuffMutex)
  StartChunkedContentTransfer(F("text/html"));
  InitChunkedContent();
  SendChunkedContent(jquery);
  SendChunkedContent(configJs);
  SendChunkedContent(style);
  SendChunkedContent(configHtml);
  FlushChunkedContent();
  CRITICALSECTION_END;
}

void OTAWebServer::postUpdateConfig()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }

  Settings newSettings(SettingsMngr.GetSettingsFile(), true);

  newSettings.mqttServer = server.arg("mqttsrvr");
  newSettings.mqttPort = server.arg("mqttport").toInt();
  newSettings.mqttUser = server.arg("mqttusr");
  newSettings.mqttPwd = server.arg("mqttpwd");

  for (int i = 0; i < server.args(); i++)
  {
    if (server.argName(i) == "mqttsrvr")
      newSettings.mqttServer = server.arg(i);
    else if (server.argName(i) == "mqttport")
      newSettings.mqttPort = server.arg(i).toInt();
    else if (server.argName(i) == "mqttusr")
      newSettings.mqttUser = server.arg(i);
    else if (server.argName(i) == "mqttpwd")
      newSettings.mqttPwd = server.arg(i);
    else if (server.argName(i) == "scanperiod")
      newSettings.scanPeriod = server.arg(i).toInt();
    else if (server.argName(i) == "whiteList")
      newSettings.EnableWhiteList(server.arg(i) == "true");
    else //other are mac address with battery check in the form "AE13FCB45BAD":"true"
    {
      bool batteryCheck = server.arg(i) == "true";
      newSettings.AddDeviceToWhiteList(server.argName(i), batteryCheck);
    }
  }

  DEBUG_PRINTLN(newSettings.toJSON().c_str());

  server.sendHeader(F("Connection"), F("close"));
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  if (newSettings.Save())
    server.send(200, F("text/html"), "Ok");
  else
    server.send(500, F("text/html"), "Error saving settings");
  server.client().flush();
  server.client().stop();
}

void OTAWebServer::InitChunkedContent()
{
  databuffer[0]='\0';
}

void OTAWebServer::SendChunkedContent(const char* content)
{
  concatAndFlush(databuffer,maxdatasize,content);
}

void OTAWebServer::FlushChunkedContent()
{
  server.sendContent_P(databuffer);
  server.sendContent_P("",0);
}

void OTAWebServer::getServerInfoData()
{
  StartChunkedContentTransfer(F("text/json"));
  CRITICALSECTION_START(dataBuffMutex)
  InitChunkedContent();
  SendChunkedContent("{");
  SendChunkedContent(R"("gateway":")" GATEWAY_NAME R"(",)");
  SendChunkedContent(R"("firmware":")" VERSION R"(",)");

#if DEVELOPER_MODE
  SendChunkedContent(R"("build":")");
  SendChunkedContent(Firmware::BuildTime);
  SendChunkedContent(R"(",)");
  SendChunkedContent(R"("memory":")");
  SendChunkedContent(String(xPortGetFreeHeapSize()).c_str());
  SendChunkedContent( R"( bytes",)");
#endif
  SendChunkedContent( R"("uptime":")");
  SendChunkedContent( formatMillis(millis()).c_str());
  SendChunkedContent( R"(",)");
  SendChunkedContent( R"("ssid":")" WIFI_SSID R"(",)");
  SendChunkedContent( R"("battery":)" xstr(PUBLISH_BATTERY_LEVEL) R"(,)");
  SendChunkedContent( R"("devices":[)");
  bool first = true;
  for (auto &trackedDevice : BLETrackedDevices)
  {
    if(first) first = false;
    else SendChunkedContent(",");
    SendChunkedContent( R"({"mac":")");
    SendChunkedContent( trackedDevice.address.c_str());
    SendChunkedContent(R"(",)");
    SendChunkedContent( R"("rssi":)");
    SendChunkedContent( trackedDevice.rssi.c_str());
    SendChunkedContent(R"(,)");
#if PUBLISH_BATTERY_LEVEL
    SendChunkedContent( R"("battery":)");
    SendChunkedContent( String(trackedDevice.batteryLevel).c_str());
    SendChunkedContent(R"(,)");
#endif
    SendChunkedContent( R"("state":")");
    SendChunkedContent( trackedDevice.isDiscovered ? "On" : "Off");
    SendChunkedContent(R"("})");
  }
  SendChunkedContent("]}");
  FlushChunkedContent();
  
  CRITICALSECTION_END
}

void OTAWebServer::getServerInfo()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }

  CRITICALSECTION_START(dataBuffMutex)
  StartChunkedContentTransfer(F("text/html"));
  InitChunkedContent();
  SendChunkedContent(jquery);
  SendChunkedContent(sysInfoJs);
  SendChunkedContent(style);
  SendChunkedContent(sysInfoHtml);
  FlushChunkedContent();
  CRITICALSECTION_END
}
/*
 * setup function
 */
void OTAWebServer::setup(const String &hN, const String &_ssid_, const String &_password_)
{

  hostName = hN;
  ssid = _ssid_;
  password = _password_;

  DEBUG_PRINTLN("WebServer Setup");
  WiFiConnect(ssid, password);

  /*use mdns for host name resolution*/
  if (!MDNS.begin(hostName.c_str()))
  { //http://bletracker
    DEBUG_PRINTLN("Error setting up MDNS responder!");
  }
  else
    DEBUG_PRINTLN("mDNS responder started");

  /*return index page which is stored in serverIndex */
  server.on(F("/"), HTTP_GET, [&]() { getIndex(); });
  
  server.on(F("/getindexdata"), HTTP_GET, [&]() { getIndexData(); });

  server.on(F("/config"), HTTP_GET, [&]() { getConfig(); });

  server.on(F("/getconfigdata"), HTTP_GET, [&]() { getConfigData(); });

  server.on(F("/updateconfig"), HTTP_POST, [&]() { postUpdateConfig(); });

  server.on(F("/serverinfo"), HTTP_GET, [&]() { getServerInfo(); });

  server.on(F("/getserverinfodata"), HTTP_GET, [&]() { getServerInfoData(); });

  server.on(F("/reset"), HTTP_GET, [&]() { resetESP32Page(); });

#if ENABLE_FILE_LOG
  server.on(F("/logs"), HTTP_GET, [&] { getLogs(); });

  server.on(F("/eraselogs"), HTTP_GET, [&] { eraseLogs(); });

  server.on(F("/getlogsdata"), HTTP_GET, [&] { getLogsData(); });
#endif

  server.on(F("/otaupdate"), HTTP_GET, [&]() { getOTAUpdate(); });

  /*handling uploading firmware file */
  server.on(
      F("/update"), HTTP_POST, [&]() {
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("text/plain"), (Update.hasError()) ? F("FAIL") : F("OK"));
    ESP.restart(); }, [&]() {
      #if ENABLE_FILE_LOG
      if(SPIFFSLogger.isEnabled())
        {
          FILE_LOG_WRITE("OTA Update started...");
          SPIFFSLogger.enabled(false);
          SPIFFS.end();
        }
      #endif
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      DEBUG_PRINTF("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        DEBUG_PRINTF("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    } });

  server.begin();
}

void WebServerLoop(void *param)
{
  if (param == NULL)
    return;

  WebServer *server = (WebServer *)param;
  while (true)
  {
    try
    {
      server->handleClient();
      delay(100);
    } 
    catch(...)
    {

    }
  }
}

void OTAWebServer::loop(void)
{

  if (serverRunning)
    return;

  xTaskCreatePinnedToCore(
      WebServerLoop,   /* Function to implement the task */
      "WebServerLoop", /* Name of the task */
      10000,           /* Stack size in words */
      (void *)&server, /* Task input parameter */
      20,              /* Priority of the task */
      NULL,            /* Task handle. */
      1);

  serverRunning = true;
}
#endif /*ENABLE_OTA_WEBSERVER*/
