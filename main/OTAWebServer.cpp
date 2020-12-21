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
#include "build_defs.h"
#include "macro_utility.h"
#include "settings.h"

extern std::vector<BLETrackedDevice> BLETrackedDevices;

const char jquery[] PROGMEM = R"=====(<script src="https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js"></script>)=====";

//Import the style.css from file
const char style[] = "<style>"
#include "html/style.css.h"
                     "</style>";

/* Server Index Page */
const char otaUpdateHtml[] PROGMEM =
#include "html/otaupdate.html.h"
    ;

//Import the config.js script from file
const char otaUpdateJs[] PROGMEM = "<script>"
#include "html/otaupdate.js.h"
                                   "</script>";

//Import the config.html page from file
const char configHtml[] PROGMEM =
#include "html/config.html.h"
    ;

//Import the config.js script from file
const char configJs[] PROGMEM = "<script>"
#include "html/config.js.h"
                                "</script>";

const char indexHtml[] PROGMEM =
#include "html/index.html.h"
    ;

const char sysInfoHtml[] PROGMEM =
#include "html/sysinfo.html.h"
    ;

const char sysInfoJs[] PROGMEM = "<script>"
#include "html/sysinfo.js.h"
                                 "</script>";

#define _CONTENT_DELAY_ 20
#define SEND_CONTENT(x) server.sendContent(x)     /*;delay(_CONTENT_DELAY_)*/
#define SEND_CONTENT_P(x) server.sendContent_P(x) /*;delay(_CONTENT_DELAY_)*/
#define START_CONTENT_TRANSFER(_content_type_)                               \
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate"); \
  server.sendHeader("Pragma", "no-cache");                                   \
  server.sendHeader("Expires", "-1");                                        \
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);                           \
  server.send(200, _content_type_, "");

#if DEVELOPER_MODE
static const char BuildTime[] =
    {
        BUILD_YEAR_CH0, BUILD_YEAR_CH1, BUILD_YEAR_CH2, BUILD_YEAR_CH3,
        '-',
        BUILD_MONTH_CH0, BUILD_MONTH_CH1,
        '-',
        BUILD_DAY_CH0, BUILD_DAY_CH1,
        'T',
        BUILD_HOUR_CH0, BUILD_HOUR_CH1,
        ':',
        BUILD_MIN_CH0, BUILD_MIN_CH1,
        ':',
        BUILD_SEC_CH0, BUILD_SEC_CH1,
        '\0'};
#endif

OTAWebServer::OTAWebServer()
    : server(80), serverRunning(false)
{
}

void OTAWebServer::resetESP32Page()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }

  server.client().setNoDelay(true);
  server.client().setTimeout(30);
  START_CONTENT_TRANSFER(F("text/html"));
  SEND_CONTENT(F("<div align='center'>Resetting...<br><progress id='g' class='y' value='0' max='100' style='align-self:center; text-align:center;'/></div>"
                 "<script>window.onload=function(){};var progval=0;var myVar=setInterval(Prog,80);"
                 "function Prog(){progval++;document.getElementById('g').value=progval;"
                 "if (progval==100){clearInterval(myVar);setTimeout(Check, 3000);}}"
                 "function Check(){if (progval==100){clearInterval(myVar);var ftimeout=setTimeout(null,5000);"
                 "window.location='/';}}</script>"));
  ESP.restart();
}

void OTAWebServer::getConfigData()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  server.sendHeader(F("Connection"), F("close"));
  DEBUG_PRINTLN(SettingsMngr.toJSON());
  if(server.args() > 0 && server.hasArg("factory") && server.arg("factory")=="true")
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
  START_CONTENT_TRANSFER(F("text/html"));
  SEND_CONTENT_P(style);
  SEND_CONTENT_P(indexHtml);
}

void OTAWebServer::getOTAUpdate()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }
  START_CONTENT_TRANSFER(F("text/html"));
  SEND_CONTENT_P(jquery);
  SEND_CONTENT_P(otaUpdateJs);
  SEND_CONTENT_P(style);
  SEND_CONTENT_P(otaUpdateHtml);
}

void OTAWebServer::getConfig()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }

  START_CONTENT_TRANSFER(F("text/html"));
  SEND_CONTENT_P(jquery);
  SEND_CONTENT_P(configJs);
  SEND_CONTENT_P(style);
  SEND_CONTENT_P(configHtml);
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


void OTAWebServer::getServerInfoData()
{
  START_CONTENT_TRANSFER(F("text/json"));
  String data = "{";
  data += R"("gateway":")" GATEWAY_NAME R"(",)";
  data += R"("firmware":")" VERSION R"(",)";
#if DEVELOPER_MODE
  data += R"("build":")" + String(BuildTime) + R"(",)";
  data += R"("memory":")" + String(xPortGetFreeHeapSize()) + R"( bytes",)";
#endif
  data += R"("uptime":")" + formatMillis(millis()) + R"(",)";
  data += R"("ssid":")" WIFI_SSID R"(",)";
  data += R"("battery":)" xstr(PUBLISH_BATTERY_LEVEL) R"(,)";
  data += R"("devices":[)";
  SEND_CONTENT(data);
  bool first = true;
  for (auto &trackedDevice : BLETrackedDevices)
  {
    data = first ? "" : ",";
    first = false;
    data += R"({"mac":")" + trackedDevice.address + R"(",)";
    data += R"("rssi":)" + trackedDevice.rssi + R"(,)";
#if PUBLISH_BATTERY_LEVEL
    data += R"("battery":)" + String(trackedDevice.batteryLevel) + R"(,)";
#endif
    data += R"("state":")" + String(trackedDevice.isDiscovered ? F("On") : F("Off")) + R"("})";
    SEND_CONTENT(data);
  }

  data = "]}";
  SEND_CONTENT(data);
}

void OTAWebServer::getServerInfo()
{
  if (!server.authenticate(WEBSERVER_USER, WEBSERVER_PASSWORD))
  {
    return server.requestAuthentication();
  }

  START_CONTENT_TRANSFER(F("text/html"));
  SEND_CONTENT_P(jquery);
  SEND_CONTENT_P(sysInfoJs);
  SEND_CONTENT_P(style);
  SEND_CONTENT_P(sysInfoHtml);
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

  server.on(F("/otaupdate"), HTTP_GET, [&]() { getOTAUpdate(); });

  server.on(F("/config"), HTTP_GET, [&]() { getConfig(); });

  server.on(F("/getconfigdata"), HTTP_GET, [&]() { getConfigData(); });

  server.on(F("/updateconfig"), HTTP_POST, [&]() { postUpdateConfig(); });

  server.on(F("/serverinfo"), HTTP_GET, [&]() { getServerInfo(); });

  server.on(F("/getserverinfodata"), HTTP_GET, [&]() { getServerInfoData(); });

  server.on(F("/reset"), HTTP_GET, [&]() { resetESP32Page(); });

  /*handling uploading firmware file */
  server.on(F("/update"), HTTP_POST, [&]() {
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("text/plain"), (Update.hasError()) ? F("FAIL") : F("OK"));
    ESP.restart(); }, [&]() {
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
    } 
  });

  server.begin();
}

void WebServerLoop(void *param)
{
  if (param == NULL)
    return;

  WebServer *server = (WebServer *)param;
  while (true)
  {
    server->handleClient();
    delay(100);
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
