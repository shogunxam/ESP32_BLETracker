#include "main.h"
#include "config.h"

#if ENABLE_OTA_WEBSERVER
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <string>
#include <atomic>

#include "WiFiManager.h"
#include "DebugPrint.h"
#include "OTAWebServer.h"
#include "macro_utility.h"
#include "settings.h"
#include <Regexp.h>

#include "SPIFFSLogger.h"

extern std::vector<BLETrackedDevice> BLETrackedDevices;

#include "html/style.css.gz.h"
#include "html/index.html.gz.h"
#include "html/index.js.gz.h"
#include "html/sysinfo.html.gz.h"
#include "html/sysinfo.js.gz.h"
#include "html/config.html.gz.h"
#include "html/config.js.gz.h"

#if ENABLE_FILE_LOG
#include "html/logs.html.gz.h"
#include "html/logs.js.gz.h"
#endif /*ENABLE_FILE_LOG*/

#include "html/otaupdate.html.gz.h"
#include "html/otaupdate.js.gz.h"

OTAWebServer::OTAWebServer()
    : server(80),
      dataBuffMutex("OTAWebServer_DataBuffer")
{
}

const size_t maxdatasize = 5 * 1024; //5Kb
static uint8_t databuffer[maxdatasize];
static size_t databufferStartPos;

//Return the number of characters written
size_t OTAWebServer::append(uint8_t *dest, size_t buffsize, size_t destStartPos, const uint8_t *src, size_t srcSize, size_t srcStartPos)
{
  size_t available = buffsize - destStartPos;
  size_t wrote = 0;
  uint8_t *dstWlkr = dest + destStartPos;
  const uint8_t *srcWlkr = src + srcStartPos;
  while ((wrote + srcStartPos) < srcSize && wrote < available)
  {
    *dstWlkr = srcWlkr[wrote];
    wrote++;
    dstWlkr++;
  }
  return wrote;
}

//Return the current position in the destination;
size_t OTAWebServer::appendAndFlush(uint8_t *dest, size_t buffsize, size_t destStartPos, const uint8_t *src, size_t srcSize)
{
  size_t wrote = append(dest, buffsize, destStartPos, src, srcSize);
  size_t totWrote = wrote;
  while (totWrote < srcSize)
  {
    //The destintion buffer is full
    server.sendContent_P((char *)dest, buffsize);
    destStartPos = 0;
    wrote = append(dest, buffsize, destStartPos, src, srcSize, wrote);
    totWrote += wrote;
  }
  return destStartPos + wrote;
}

void OTAWebServer::StartChunkedContentTransfer(const char *contentType, bool zipped)
{
  server.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
  server.sendHeader(F("Pragma"), F("no-cache"));
  server.sendHeader(F("Expires"), F("-1"));
  server.sendHeader(F("Transfer-Encoding"), F("chunked"));
  if (zipped)
    server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, contentType, "");
  databufferStartPos = 0;
}

void OTAWebServer::SendChunkedContent(const uint8_t *content, size_t size)
{
  databufferStartPos = appendAndFlush(databuffer, maxdatasize, databufferStartPos, content, size);
}

void OTAWebServer::SendChunkedContent(const char *content)
{
  databufferStartPos = appendAndFlush(databuffer, maxdatasize, databufferStartPos, (const uint8_t *)content, strlen(content));
}

void OTAWebServer::FlushChunkedContent()
{
  if (databufferStartPos > 0)
    server.sendContent_P((char *)databuffer, databufferStartPos);
  server.sendContent_P("", 0);
}

void OTAWebServer::resetESP32Page()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  LOG_TO_FILE_I("User request for restart...");
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
  server.client().setNoDelay(true);
  server.sendHeader("Connection", "close");
  server.send(200, "text/html", "Ok");
}

void OTAWebServer::getLogsData()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  CRITICALSECTION_START(SPIFFSLogger)
  CRITICALSECTION_START(dataBuffMutex)
  StartChunkedContentTransfer("text/json");
  SPIFFSLoggerClass::logEntry entry;
  SPIFFSLogger.read_logs_start(true);
  bool first = true;
  int count = 0;

  SendChunkedContent("[");

  while (SPIFFSLogger.read_next_entry(entry))
  {
    if (first)
      first = false;
    else
      SendChunkedContent(",");
    SendChunkedContent(R"({"t":")");
    SendChunkedContent(entry.timeStamp);
    SendChunkedContent(R"(","m":")");
    SendChunkedContent(entry.msg);
    SendChunkedContent(R"("})");
  }

  SendChunkedContent("]");
  FlushChunkedContent();
  CRITICALSECTION_END; //dataBuffMutex
  CRITICALSECTION_END; //SPIFFSLogger
}

void OTAWebServer::getLogsJs()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/javascripthtml", (const char *)logs_js_gz, logs_js_gz_size);
}

void OTAWebServer::getLogs()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/html", (const char *)logs_html_gz, logs_html_gz_size);
}

void OTAWebServer::postLogs()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }

  server.client().setNoDelay(true);
  if (server.args() == 1 && server.hasArg("loglevel"))
  {
    Settings newSettings(SettingsMngr.GetSettingsFile(), true);
    newSettings = SettingsMngr;
    newSettings.logLevel = server.arg("loglevel").toInt();
    SettingsMngr.logLevel = newSettings.logLevel;
    SPIFFSLogger.setLogLevel(SPIFFSLoggerClass::LogLevel(SettingsMngr.logLevel));
    server.sendHeader(F("Connection"), F("close"));
    if (newSettings.Save())
    {
      LOG_TO_FILE_I("New LogLevel configuration successfully saved.");
      server.send(200, F("text/html"), "Ok");
    }
    else
    {
      LOG_TO_FILE_E("Error saving the new LogLevel configuration.");
      server.send(500, F("text/html"), "Error saving settings");
    }
    return;
  }

  server.sendHeader(F("Connection"), F("close"));
  server.send(400, F("text/html"), "Bad request");
}

#endif /*ENABLE_FILE_LOG*/

void OTAWebServer::getStyle()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }

  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/css", (const char *)style_css_gz, style_css_gz_size);
}

void OTAWebServer::getIndexJs()
{
  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/javascript", (const char *)index_js_gz, index_js_gz_size);
}

void OTAWebServer::getIndex()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/html", (const char *)index_html_gz, index_html_gz_size);
}

void OTAWebServer::getIndexData()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  CRITICALSECTION_START(dataBuffMutex)
  StartChunkedContentTransfer("text/json");
  SendChunkedContent(R"({"gateway":")" GATEWAY_NAME R"(","ver":")" VERSION R"(","logs":)");
#if ENABLE_FILE_LOG
  SendChunkedContent("true");
#else
  SendChunkedContent("false");
#endif
  SendChunkedContent("}");
  FlushChunkedContent();
  CRITICALSECTION_END
}

void OTAWebServer::getOTAUpdateJs()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/javascript", (const char *)otaupdate_js_gz, otaupdate_js_gz_size);
}

void OTAWebServer::getOTAUpdate()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/html", (const char *)otaupdate_html_gz, otaupdate_html_gz_size);
}

void OTAWebServer::getConfigData()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  DEBUG_PRINTLN(SettingsMngr.toJSON());
  if (server.args() > 0 && server.hasArg("factory") && server.arg("factory") == "true")
  {
    Settings factoryValues;
    server.send(200, F("text/json"), factoryValues.toJSON());
    return;
  }

  server.send(200, F("text/json"), SettingsMngr.toJSON());
  //server.client().stop();
}

void OTAWebServer::getConfigJs()
{
  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/javascript", (const char *)config_js_gz, config_js_gz_size);
}

void OTAWebServer::getConfig()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/html", (const char *)config_html_gz, config_html_gz_size);
}

void OTAWebServer::getUpdateBattery()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }

  if (server.hasArg("mac"))
  {
    DEBUG_PRINTF("Force Battery Update for: %s\n", server.arg("mac").c_str());
    ForceBatteryRead(server.arg("mac").c_str());
  }

  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.send(200, F("text/html"), "Ok");
}

void OTAWebServer::postUpdateConfig()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }

  server.client().setNoDelay(true);
  Settings newSettings(SettingsMngr.GetSettingsFile(), true);
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
    else if (server.argName(i) == "maxNotAdvPeriod")
      newSettings.maxNotAdvPeriod = server.arg(i).toInt();
    else if (server.argName(i) == "whiteList")
      newSettings.EnableWhiteList(server.arg(i) == "true");
    else //other are mac address with battery check in the form "AE13FCB45BAD":"true"
    {
        MatchState ms((char *)server.argName(i).c_str());
        if (ms.Match(R"((%w+)%[(%w+)%])") == REGEXP_MATCHED)
        {
          DEBUG_PRINTF("Match %s\n", server.argName(i).c_str())
          char buff[ADDRESS_STRING_SIZE];
          ms.GetCapture(buff, 0);
          Settings::KnownDevice* device = newSettings.GetDevice(buff);
          if(device == nullptr)
          {
              Settings::KnownDevice tDev;
              snprintf(tDev.address, ADDRESS_STRING_SIZE, "%s", buff);
              newSettings.AddDeviceToList(tDev);
              device = newSettings.GetDevice(buff);
          }
          DEBUG_PRINTF("Match value %s\n", server.arg(i).c_str())
          ms.GetCapture(buff, 1);
          if(strcmp(buff,"batt") == 0)
            device->readBattery = server.arg(i) == "true";
          else if (strcmp(buff,"desc") == 0)
            snprintf(device->description, DESCRIPTION_STRING_SIZE,"%s",server.arg(i));
        }
    }
  }

  DEBUG_PRINTLN(newSettings.toJSON().c_str());

  server.sendHeader(F("Connection"), F("close"));
  if (newSettings.Save())
  {
    LOG_TO_FILE_I("New configuration successfully saved.");
    server.send(200, F("text/html"), "Ok");
  }
  else
  {
    LOG_TO_FILE_E("Error saving the new configuration.");
    server.send(500, F("text/html"), "Error saving settings");
  }

}


void OTAWebServer::getSysInfoData()
{
  CRITICALSECTION_START(dataBuffMutex)
  char strbuff[20];
  server.client().setNoDelay(true);
  StartChunkedContentTransfer("text/json");
  SendChunkedContent("{");
  SendChunkedContent(R"("gateway":")" GATEWAY_NAME R"(",)");
  SendChunkedContent(R"("firmware":")" VERSION R"(",)");

#if DEVELOPER_MODE
  SendChunkedContent(R"("build":")");
  SendChunkedContent(Firmware::BuildTime);
  SendChunkedContent(R"(",)");
  SendChunkedContent(R"("memory":")");
  itoa(xPortGetFreeHeapSize(), strbuff, 10);
  SendChunkedContent(strbuff);
  SendChunkedContent(R"( bytes",)");
#endif
  SendChunkedContent(R"("uptime":")");
  SendChunkedContent(formatMillis(millis(), strbuff));
  SendChunkedContent(R"(",)");
  SendChunkedContent(R"("ssid":")" WIFI_SSID R"(",)");
  SendChunkedContent(R"("battery":)" xstr(PUBLISH_BATTERY_LEVEL) R"(,)");
  SendChunkedContent(R"("devices":[)");

  bool first = true;
  for (auto &trackedDevice : BLETrackedDevices)
  {
    if (first)
      first = false;
    else
      SendChunkedContent(",");
    SendChunkedContent(R"({"mac":")");
    SendChunkedContent(trackedDevice.address);
    SendChunkedContent(R"(",)");
    SendChunkedContent(R"("rssi":)");
    itoa(trackedDevice.rssiValue, strbuff, 10);
    SendChunkedContent(strbuff);
    SendChunkedContent(R"(,)");
#if PUBLISH_BATTERY_LEVEL
    SendChunkedContent(R"("battery":)");
    itoa(trackedDevice.batteryLevel, strbuff, 10);
    SendChunkedContent(strbuff);
    SendChunkedContent(R"(,)");
#endif
    SendChunkedContent(R"("state":")");
    SendChunkedContent(trackedDevice.isDiscovered ? "On" : "Off");
    SendChunkedContent(R"("})");
  }
  SendChunkedContent("]}");
  FlushChunkedContent();
  CRITICALSECTION_END
}

void OTAWebServer::getSysInfoJs()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/javascript", (const char *)sysinfo_js_gz, sysinfo_js_gz_size);
}

void OTAWebServer::getSysInfo()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/html", (const char *)sysinfo_html_gz, sysinfo_html_gz_size);
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

  server.on(F("/index.js"), HTTP_GET, [&]() { getIndexJs(); });

  server.on(F("/style.css"), HTTP_GET, [&]() { getStyle(); });

  server.on(F("/getindexdata"), HTTP_GET, [&]() { getIndexData(); });

  server.on(F("/config"), HTTP_GET, [&]() { getConfig(); });

  server.on(F("/config.js"), HTTP_GET, [&]() { getConfigJs(); });

  server.on(F("/getconfigdata"), HTTP_GET, [&]() { getConfigData(); });

  server.on(F("/updatebattery"), HTTP_GET, [&]() { getUpdateBattery(); });

  server.on(F("/updateconfig"), HTTP_POST, [&]() { postUpdateConfig(); });

  server.on(F("/sysinfo"), HTTP_GET, [&]() { getSysInfo(); });

  server.on(F("/sysinfo.js"), HTTP_GET, [&]() { getSysInfoJs(); });

  server.on(F("/getsysinfodata"), HTTP_GET, [&]() { getSysInfoData(); });

  server.on(F("/restart"), HTTP_GET, [&]() { resetESP32Page(); });

#if ENABLE_FILE_LOG
  server.on(F("/logs"), HTTP_GET, [&] { getLogs(); });

  server.on(F("/logs"), HTTP_POST, [&] { postLogs(); });

  server.on(F("/logs.js"), HTTP_GET, [&] { getLogsJs(); });

  server.on(F("/eraselogs"), HTTP_GET, [&] { eraseLogs(); });

  server.on(F("/getlogsdata"), HTTP_GET, [&] { getLogsData(); });
#endif

  server.on(F("/otaupdate"), HTTP_GET, [&]() { getOTAUpdate(); });
  server.on(F("/otaupdate.js"), HTTP_GET, [&]() { getOTAUpdateJs(); });

  /*handling uploading firmware file */
  server.on(
      F("/update"), HTTP_POST, [&]() {
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("text/plain"), (Update.hasError()) ? F("FAIL") : F("OK"));
    ESP.restart(); }, [&]() {
#if ENABLE_FILE_LOG
      if(SPIFFSLogger.isEnabled())
        {
          LOG_TO_FILE_I("OTA Update started...");
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

static std::atomic_ulong WebServerWatchDogStartTime(0);
void WebServerWatchDogloop(void * param)
{
    WebServer *server = (WebServer *)param;
    WebServerWatchDogStartTime.store(millis());
    while (true)
    {
        if (millis() > (WebServerWatchDogStartTime.load() + 10000))
        {
            server->stop();
            server->begin();
            DEBUG_PRINTLN("INFO: WebServerWatchdog resart server");
            LOG_TO_FILE_E("Error: WebServerWatchdog resart server");
        }
        delay(1000);
    };
}

void WebServerWatchDogFeed()
{
    WebServerWatchDogStartTime.store(millis());
}

void WebServerLoop(void *param)
{
  bool restart = false;
  if (param == NULL)
    return;

  WebServer *server = (WebServer *)param;
  while (true)
  {
    try
    {
      server->handleClient();
      WebServerWatchDogFeed();
      if(restart)
      {
        restart = false;
        server->stop();
        server->begin();
      }
      delay(100);
    }
    catch (std::exception &e)
    {
      restart = true;
      DEBUG_PRINTF("Error Caught Exception %s", e.what());
      LOG_TO_FILE_E("Error Caught Exception %s", e.what());
    }
    catch (...)
    {
      restart = true;
      DEBUG_PRINTLN("Error Unhandled exception trapped in webserver loop");
      LOG_TO_FILE_E("Error Unhandled exception trapped in webserver loop");
    }
  }
}

void OTAWebServer::begin(void)
{
  xTaskCreatePinnedToCore(
      WebServerLoop,   /* Function to implement the task */
      "WebServerLoop", /* Name of the task */
      4096,            /* Stack size in words */
      (void *)&server, /* Task input parameter */
      10,              /* Priority of the task */
      NULL,            /* Task handle. */
      1);

  xTaskCreatePinnedToCore(
      WebServerWatchDogloop,   /* Function to implement the task */
      "WebServerWatchDogloop", /* Name of the task */
      3072,           /* Stack size in words */
      (void *)&server, /* Task input parameter */
      10,             /* Priority of the task */
      NULL,           /* Task handle. */
      1);
}
#endif /*ENABLE_OTA_WEBSERVER*/
