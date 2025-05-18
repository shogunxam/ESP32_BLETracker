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
#include <map>

#include "WiFiManager.h"
#include "DebugPrint.h"
#include "OTAWebServer.h"
#include "macro_utility.h"
#include "settings.h"
#include "SPIFFSLogger.h"

extern std::vector<BLETrackedDevice> BLETrackedDevices;

#include "html/style.css.gz.h"
#include "html/utility.js.gz.h"
#include "html/index.html.gz.h"
#include "html/index.js.gz.h"
#include "html/sysinfo.html.gz.h"
#include "html/sysinfo.js.gz.h"
#include "html/config.html.gz.h"
#include "html/config.js.gz.h"

#if USE_MQTT
#include "html/config_mqtt.html.gz.h"
#endif
#if USE_UDP
#include "html/config_udp.html.gz.h"
#endif

#if ENABLE_FILE_LOG
#include "html/logs.html.gz.h"
#include "html/logs.js.gz.h"
#endif /*ENABLE_FILE_LOG*/

#include "html/otaupdate.html.gz.h"
#include "html/otaupdate.js.gz.h"

#include "html/reset.html.gz.h"

#include "ESPUtility.h"

OTAWebServer::OTAWebServer()
    : server(80),
      dataBuffMutex("OTAWebServer_DataBuffer")
{
}

const size_t maxdatasize = 5 * 1024; // 5Kb
static uint8_t databuffer[maxdatasize];
static size_t databufferStartPos;

// Return the number of characters written
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

// Return the current position in the destination;
size_t OTAWebServer::appendAndFlush(uint8_t *dest, size_t buffsize, size_t destStartPos, const uint8_t *src, size_t srcSize)
{
  size_t wrote = append(dest, buffsize, destStartPos, src, srcSize);
  size_t totWrote = wrote;
  while (totWrote < srcSize)
  {
    // The destintion buffer is full
    server.sendContent_P((char *)dest, buffsize);
    destStartPos = 0;
    wrote = append(dest, buffsize, destStartPos, src, srcSize, wrote);
    totWrote += wrote;
  }
  return destStartPos + wrote;
}

void OTAWebServer::SendDefaulHeaders()
{
  server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server.sendHeader(F("Access-Control-Allow-Methods"), F("GET, POST, OPTIONS"));
  server.sendHeader(F("Access-Control-Allow-Headers"), F("Content-Type, Authorization, Cache-Control, Pragma, Expires"));
  server.sendHeader(F("Access-Control-Max-Age"), F("3600"));
  server.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate , max-age=0"));
  server.sendHeader(F("Pragma"), F("no-cache"));
  server.sendHeader(F("Expires"), F("-1"));
  server.sendHeader(F("Connection"), F("close"));
}

void OTAWebServer::StartChunkedContentTransfer(const char *contentType, bool zipped)
{
  SendDefaulHeaders();

  // Header "Transfer-Encoding:chunked" is automatically added when content length is set to unknown
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
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }
  LOG_TO_FILE_I("User request for restart...");

  server.client().setNoDelay(true);
  SendDefaulHeaders();
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/html", (const char *)reset_html_gz, reset_html_gz_size);
  server.client().flush();

  ESPUtility::scheduleReset(2000);
}

#if ENABLE_FILE_LOG
void OTAWebServer::eraseLogs()
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }

  SPIFFSLogger.clearLog();
  server.client().setNoDelay(true);
  SendDefaulHeaders();
  server.send(200, "text/html", "Ok");
}

void OTAWebServer::getLogsData()
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
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
  char strbuff[20];
  while (SPIFFSLogger.read_next_entry(entry))
  {
    if (first)
      first = false;
    else
      SendChunkedContent(",");
    SendChunkedContent(R"({"t":)");
    SendChunkedContent(itoa(entry.timeStamp, strbuff, 10));
    SendChunkedContent(R"(,"m":")");
    SendChunkedContent(entry.msg);
    SendChunkedContent(R"(","l":)");
    SendChunkedContent(itoa(entry.level, strbuff, 10));
    SendChunkedContent(R"(})");
  }

  SendChunkedContent("]");
  FlushChunkedContent();
  CRITICALSECTION_END; // dataBuffMutex
  CRITICALSECTION_END; // SPIFFSLogger
}

void OTAWebServer::getLogsJs()
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  SendDefaulHeaders();

  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/javascripthtml", (const char *)logs_js_gz, logs_js_gz_size);
}

void OTAWebServer::getLogs()
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
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
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
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
    SendDefaulHeaders();
    if (newSettings.Save())
    {
      LOG_TO_FILE_I("New LogLevel configuration successfully saved.");
      DEBUG_PRINTF("Log level set to %d\n", newSettings.logLevel);
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
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }

  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/css", (const char *)style_css_gz, style_css_gz_size);
}

void OTAWebServer::getUtilityJs()
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }

  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/javascript", (const char *)utility_js_gz, utility_js_gz_size);
}

void OTAWebServer::getIndexJs()
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }

  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/javascript", (const char *)index_js_gz, index_js_gz_size);
}

void OTAWebServer::getIndex()
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
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
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);

  CRITICALSECTION_START(dataBuffMutex)
  StartChunkedContentTransfer("text/json");
  SendChunkedContent(R"({"gateway":")");
  SendChunkedContent(SettingsMngr.gateway.c_str());
  SendChunkedContent(R"(","ver":")" VERSION R"(","logs":)");
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
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
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
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
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
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);

  CRITICALSECTION_START(dataBuffMutex)
  char strbuff[20];

  StartChunkedContentTransfer("text/json");

  DEBUG_PRINTLN(SettingsMngr.toJSON());
  if (server.args() > 0 && server.hasArg("factory") && server.arg("factory") == "true")
  {
    Settings factoryValues;
    SendChunkedContent(factoryValues.toJSON().c_str());
  }
  else
  {
    SendChunkedContent(SettingsMngr.toJSON().c_str());
  }
  FlushChunkedContent();

  CRITICALSECTION_END
}

void OTAWebServer::getConfigJs()
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }

  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/javascript", (const char *)config_js_gz, config_js_gz_size);
}

void OTAWebServer::getConfig()
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
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
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }

  SendDefaulHeaders();

  if (server.hasArg("mac"))
  {
    DEBUG_PRINTF("Force Battery Update for: %s\n", server.arg("mac").c_str());
    ForceBatteryRead(server.arg("mac").c_str());
  }

  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.send(200, F("text/html"), "Ok");
}

using ParamHandler = std::function<void(const String &)>;

void OTAWebServer::postUpdateConfig()
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }

  server.client().setNoDelay(true);
  Settings newSettings(SettingsMngr.GetSettingsFile(), true);

  std::map<String, ParamHandler> paramHandlers = {
      {"wbsusr", [&](const String &val)
       { newSettings.wbsUser = val; }},
      {"wbspwd", [&](const String &val)
       { newSettings.wbsPwd = val; }},
      {"ssid", [&](const String &val)
       { newSettings.wifiSSID = val; }},
      {"wifipwd", [&](const String &val)
       { newSettings.wifiPwd = val; }},
      {"gateway", [&](const String &val)
       { newSettings.gateway = val; }},
      {"mqttsrvr", [&](const String &val)
       { newSettings.serverAddr = val; }},
      {"mqttport", [&](const String &val)
       { newSettings.serverPort = atoi(val.c_str()); }},
      {"mqttusr", [&](const String &val)
       { newSettings.serverUser = val; }},
      {"mqttpwd", [&](const String &val)
       { newSettings.serverPwd = val; }},
      {"scanperiod", [&](const String &val)
       { newSettings.scanPeriod = atoi(val.c_str()); }},
      {"maxNotAdvPeriod", [&](const String &val)
       { newSettings.maxNotAdvPeriod = atoi(val.c_str()); }},
      {"whiteList", [&](const String &val)
       { newSettings.EnableWhiteList(val == "true"); }},
      {"manualscan", [&](const String &val)
       { newSettings.EnableManualScan(val == "true"); }}};

  for (int i = 0; i < server.args(); i++)
  {
    const auto argName = server.argName(i);
    const auto argValue = server.arg(i);
    DEBUG_PRINTF("%s = %s \n", argName.c_str(), argValue.c_str());

    auto it = paramHandlers.find(argName);
    if (it != paramHandlers.end())
    {
      it->second(argValue);
    }
    else
    {
      const char *input = argName.c_str();

      // Find opening bracket '['
      const char *openBracket = static_cast<const char *>(memchr(input, '[', strlen(input)));
      if (!openBracket)
        continue;

      // Find closing bracket ']'
      const char *closeBracket = static_cast<const char *>(memchr(openBracket, ']', strlen(openBracket)));
      if (!closeBracket || closeBracket <= openBracket)
        continue;

      // Extract MAC address (before '[')
      size_t macLen = openBracket - input;
      char mac[ADDRESS_STRING_SIZE];
      memcpy(mac, input, macLen);
      mac[macLen] = '\0'; // Null-terminate

      // Get or create device by MAC address
      Settings::KnownDevice *device = newSettings.GetDevice(mac);
      if (!device)
      {
        Settings::KnownDevice tDev;
        memcpy(tDev.address, mac, std::min(sizeof(tDev.address) - 1, macLen));
        tDev.address[std::min(sizeof(tDev.address) - 1, macLen)] = '\0';
        newSettings.AddDeviceToList(tDev);
        device = newSettings.GetDevice(mac);
      }

      if (!device)
        continue;

      // Extract property name between '[' and ']'
      size_t propLen = closeBracket - openBracket - 1;
      const char *prop = openBracket + 1;

      // Get value of the parameter
      const char *value = argValue.c_str();

      // Fast property check using memcmp instead of strcmp
      if (propLen == 4 && memcmp(prop, "desc", 4) == 0)
      {
        // Set description safely
        strncpy(device->description, value, DESCRIPTION_STRING_SIZE - 1);
        device->description[DESCRIPTION_STRING_SIZE - 1] = '\0';
      }
      else if (propLen == 4 && memcmp(prop, "batt", 4) == 0)
      {
        // Set readBattery flag with minimal comparison
        device->readBattery = (value[0] == 't' || value[0] == 'T');
      }
    }
  }
  
  DEBUG_PRINTLN(newSettings.toJSON().c_str());

  SendDefaulHeaders();
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

void OTAWebServer::sendSysInfoData(bool trackerInfo, bool deviceList)
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }

  CRITICALSECTION_START(dataBuffMutex)
  char strbuff[20];
  server.client().setNoDelay(true);

  StartChunkedContentTransfer("text/json");
  SendChunkedContent("{");
  if (trackerInfo)
  {
    SendChunkedContent(R"("gateway":")");
    SendChunkedContent(SettingsMngr.gateway.c_str());
    SendChunkedContent(R"(",)");
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
    SendChunkedContent(R"("ssid":")");
    SendChunkedContent(SettingsMngr.wifiSSID.c_str());
    SendChunkedContent(R"(",)");
    SendChunkedContent(R"("macaddr":")");
    SendChunkedContent(WiFi.macAddress().c_str());
    SendChunkedContent(R"(",)");
    SendChunkedContent(R"("battery":)" xstr(PUBLISH_BATTERY_LEVEL));
  }
  if (trackerInfo && deviceList)
  {
    SendChunkedContent(",");
  }

  if (deviceList)
  {
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
      Settings::KnownDevice *device = SettingsMngr.GetDevice(trackedDevice.address);
      if (device != nullptr && device->description[0] != '\0')
      {
        SendChunkedContent(R"("name":")");
        SendChunkedContent(device->description);
        SendChunkedContent(R"(",)");
      }

      SendChunkedContent(R"("rssi":)");
      itoa(trackedDevice.rssiValue, strbuff, 10);
      SendChunkedContent(strbuff);
      SendChunkedContent(R"(,)");
#if PUBLISH_BATTERY_LEVEL
      SendChunkedContent(R"("battery":)");
      if (SettingsMngr.InBatteryList(trackedDevice.address))
      {
        itoa(trackedDevice.batteryLevel, strbuff, 10);
      }
      else
      {
        strcpy(strbuff, "null");
      }
      SendChunkedContent(strbuff);
      SendChunkedContent(R"(,)");
      if (trackedDevice.lastBattMeasureTime > 0)
      {
        SendChunkedContent(R"("bttime":)");
        itoa(trackedDevice.lastBattMeasureTime, strbuff, 10);
        SendChunkedContent(strbuff);
        SendChunkedContent(R"(,)");
      }
#endif
      SendChunkedContent(R"("state":")");
      SendChunkedContent(trackedDevice.isDiscovered ? "On" : "Off");
      SendChunkedContent(R"("})");
    }
    SendChunkedContent("]}");
  }
  FlushChunkedContent();
  CRITICALSECTION_END
}

void OTAWebServer::getSysInfoData()
{
  sendSysInfoData(true, true);
}

void OTAWebServer::getDevices()
{
  return sendSysInfoData(false, true);
}

void OTAWebServer::getDeviceInfoData()
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }

  // Verifica il parametro MAC
  if (!server.hasArg("mac"))
  {
    server.sendHeader(F("Connection"), F("close"));
    server.send(400, F("application/json"), F("{\"error\":\"Missing mac parameter\"}"));
    return;
  }

  String macAddress = server.arg("mac");

  // Trova il dispositivo nell'elenco dei dispositivi tracciati
  BLETrackedDevice *targetDevice = nullptr;
  for (auto &trackedDevice : BLETrackedDevices)
  {
    if (strcasecmp(trackedDevice.address, macAddress.c_str()) == 0)
    {
      targetDevice = &trackedDevice;
      break;
    }
  }

  // Se il dispositivo non è stato trovato
  if (targetDevice == nullptr)
  {
    server.sendHeader(F("Connection"), F("close"));
    server.send(404, F("application/json"), F("{\"error\":\"Device not found\"}"));
    return;
  }

  char strbuff[20];

  // Inizia il trasferimento chunked
  CRITICALSECTION_START(dataBuffMutex)
  server.client().setNoDelay(true);
  StartChunkedContentTransfer("application/json");

  // Ottieni informazioni aggiuntive dal registro dei dispositivi noti
  Settings::KnownDevice *knownDevice = SettingsMngr.GetDevice(targetDevice->address);

  // Inizia il JSON
  SendChunkedContent("{");

  // MAC
  SendChunkedContent(R"("mac":")");
  SendChunkedContent(targetDevice->address);
  SendChunkedContent(R"(",)");

  // Stato
  SendChunkedContent(R"("state":")");
  SendChunkedContent(targetDevice->isDiscovered ? "On" : "Off");
  SendChunkedContent(R"(",)");

  // RSSI
  SendChunkedContent(R"("rssi":)");
  itoa(targetDevice->rssiValue, strbuff, 10);
  SendChunkedContent(strbuff);

  // Nome/descrizione se disponibile
  if (knownDevice != nullptr && knownDevice->description[0] != '\0')
  {
    SendChunkedContent(R"(,)");
    SendChunkedContent(R"("name":")");
    SendChunkedContent(knownDevice->description);
    SendChunkedContent(R"(")");
  }

#if PUBLISH_BATTERY_LEVEL
  // Livello batteria
  SendChunkedContent(R"(,)");
  SendChunkedContent(R"("battery":)");
  itoa(targetDevice->batteryLevel, strbuff, 10);
  SendChunkedContent(strbuff);

  // Timestamp dell'ultima lettura della batteria
  if (targetDevice->lastBattMeasureTime > 0)
  {
    SendChunkedContent(R"(,)");
    SendChunkedContent(R"("lastBatteryReading":)");
    itoa(targetDevice->lastBattMeasureTime, strbuff, 10);
    SendChunkedContent(strbuff);
  }

  // Indica se la lettura della batteria è configurata
  if (knownDevice != nullptr)
  {
    SendChunkedContent(R"(,)");
    SendChunkedContent(R"("batteryReadingEnabled":)");
    SendChunkedContent(knownDevice->readBattery ? "true" : "false");
  }
#endif

  // Chiudi il JSON
  SendChunkedContent("}");

  // Concludi il trasferimento chunked
  FlushChunkedContent();
  CRITICALSECTION_END
}

void OTAWebServer::getSysInfoJs()
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
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
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Content-Encoding"), F("gzip"));
  server.send_P(200, "text/html", (const char *)sysinfo_html_gz, sysinfo_html_gz_size);
}

void OTAWebServer::setManualScan()
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }

  server.client().setNoDelay(true);
  SendDefaulHeaders();

  if (SettingsMngr.manualScan != eManualSCanMode::eManualSCanModeDisabled) // Manual scan is enabled
  {
    String url = server.uri();
    DEBUG_PRINTF("Manual Scan URL: %s", url.c_str());

    if (url == "/api/scan/on")
    {
      SettingsMngr.manualScan = eManualSCanMode::eManualSCanModeOn;
      server.send(200, "text/html", "Manual Scan On");
    }
    else if (url == "/api/scan/off")
    {
      SettingsMngr.manualScan = eManualSCanMode::eManualSCanModeOff;
      server.send(200, "text/html", "Manual Scan Off");
    }
    else
    {
      server.send(400, "text/html", "Bad Request");
    }
  }
  else
  {
    server.send(400, "text/html", "Manual Scan not enabled");
  }
}

void OTAWebServer::handleMQTTFrag()
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  SendDefaulHeaders();
#if USE_MQTT
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html", (const char *)config_mqtt_html_gz, config_mqtt_html_gz_size);
#else
  server.send(204, "text/plain", "");
#endif
}

void OTAWebServer::handleUDPFrag()
{
  if (!server.authenticate(SettingsMngr.wbsUser.c_str(), SettingsMngr.wbsPwd.c_str()))
  {
    return server.requestAuthentication();
  }
  server.client().setNoDelay(true);
  SendDefaulHeaders();
#if USE_UDP
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html", (const char *)config_udp_html_gz, config_udp_html_gz_size);
#else
  server.send(204, "text/plain", "");
#endif
}

void OTAWebServer::handleOptions()
{
  SendDefaulHeaders();
  server.send(204); // Risposta "No Content" per la preflight
}

static String sanitizeHostname(String hostname)
{
  hostname.toLowerCase();
  String sanitizedHostname = "";
  for (int i = 0; i < hostname.length(); i++)
  {
    char c = hostname.charAt(i);
    if (std::isalnum(c) || c == '-')
    {
      sanitizedHostname += c;
    }
  }

  while (sanitizedHostname.startsWith("-"))
  {
    sanitizedHostname.remove(0, 1);
  }
  while (sanitizedHostname.endsWith("-") && sanitizedHostname.length() > 0)
  {
    sanitizedHostname.remove(sanitizedHostname.length() - 1, 1);
  }

  return sanitizedHostname;
}

void OTAWebServer::enableMDNS()
{
  if (MDNS.begin(hostName.c_str()))
  {
    MDNS.addService("http", "tcp", 80);
    DEBUG_PRINTF("mDNS responder started  %s\n", hostName.c_str());
  }
  else
  {
    DEBUG_PRINTLN("Error setting up MDNS responder!");
  }
}

/*
 * setup function
 */
void OTAWebServer::setup(const String &hN)
{
  hostName = sanitizeHostname(hN);
  DEBUG_PRINTLN("WebServer Setup");

  /*use mdns for host name resolution*/
  auto callback = std::bind(&OTAWebServer::enableMDNS, this);
  WiFiManager::AddWiFiConnectedCallback(callback);

  /*return index page which is stored in serverIndex */
  server.on(F("/"), HTTP_GET, [&]()
            { getIndex(); });
  server.on(F("/"), HTTP_OPTIONS, [&]()
            { handleOptions(); });

  server.on(F("/utility.js"), HTTP_GET, [&]()
            { getUtilityJs(); });

  server.on(F("/index.js"), HTTP_GET, [&]()
            { getIndexJs(); });

  server.on(F("/style.css"), HTTP_GET, [&]()
            { getStyle(); });

  server.on(F("/getindexdata"), HTTP_GET, [&]()
            { getIndexData(); });
  server.on(F("/getindexdata"), HTTP_OPTIONS, [&]()
            { handleOptions(); });

  server.on(F("/config"), HTTP_GET, [&]()
            { getConfig(); });

  server.on(F("/config.js"), HTTP_GET, [&]()
            { getConfigJs(); });

  server.on(F("/getconfigdata"), HTTP_GET, [&]()
            { getConfigData(); });
  server.on(F("/getconfigdata"), HTTP_OPTIONS, [&]()
            { handleOptions(); });

  server.on(F("/updatebattery"), HTTP_GET, [&]()
            { getUpdateBattery(); });
  server.on(F("/updatebattery"), HTTP_OPTIONS, [&]()
            { handleOptions(); });

  server.on(F("/updateconfig"), HTTP_POST, [&]()
            { postUpdateConfig(); });
  server.on(F("/updateconfig"), HTTP_OPTIONS, [&]()
            { handleOptions(); });

  server.on(F("/sysinfo"), HTTP_GET, [&]()
            { getSysInfo(); });

  server.on(F("/sysinfo.js"), HTTP_GET, [&]()
            { getSysInfoJs(); });

  server.on(F("/getsysinfodata"), HTTP_GET, [&]()
            { getSysInfoData(); });
  server.on(F("/getsysinfodata"), HTTP_OPTIONS, [&]()
            { handleOptions(); });
  server.on(F("/api/devices"), HTTP_GET, [&]()
            { getDevices(); });

  server.on(F("/restart"), HTTP_GET, [&]()
            { resetESP32Page(); });
  server.on(F("/restart"), HTTP_OPTIONS, [&]()
            { handleOptions(); });

  server.on(F("/api/scan/on"), HTTP_POST, [&]()
            { setManualScan(); });
  server.on(F("/api/scan/off"), HTTP_POST, [&]()
            { setManualScan(); });

  server.on(F("/api/device"), HTTP_GET, [&]()
            { getDeviceInfoData(); });
  server.on(F("/api/device"), HTTP_OPTIONS, [&]()
            { handleOptions(); });

  server.on("/mqtt_config_fragment", HTTP_GET, [&]()
            { handleMQTTFrag(); });
  server.on(F("/mqtt_config_fragment"), HTTP_OPTIONS, [&]()
            { handleOptions(); });
  server.on("/udp_config_fragment", HTTP_GET, [&]()
            { handleUDPFrag(); });
  server.on(F("/udp_config_fragment"), HTTP_OPTIONS, [&]()
            { handleOptions(); });
#if ENABLE_FILE_LOG
  server.on(F("/logs"), HTTP_GET, [&]
            { getLogs(); });

  server.on(F("/logs"), HTTP_POST, [&]
            { postLogs(); });

  server.on(F("/logs.js"), HTTP_GET, [&]
            { getLogsJs(); });

  server.on(F("/logs"), HTTP_OPTIONS, [&]()
            { handleOptions(); });
  server.on(F("/eraselogs"), HTTP_GET, [&]
            { eraseLogs(); });
  server.on(F("/eraselogs"), HTTP_OPTIONS, [&]()
            { handleOptions(); });

  server.on(F("/getlogsdata"), HTTP_GET, [&]
            { getLogsData(); });
  server.on(F("/getlogsdata"), HTTP_OPTIONS, [&]()
            { handleOptions(); });
#endif

  server.on(F("/otaupdate"), HTTP_GET, [&]()
            { getOTAUpdate(); });
  server.on(F("/otaupdate.js"), HTTP_GET, [&]()
            { getOTAUpdateJs(); });

  /*handling uploading firmware file */
  server.on(
      F("/update"), HTTP_POST, [&]()
      {
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("text/plain"), (Update.hasError()) ? F("FAIL") : F("OK"));
    ESPUtility::scheduleReset(300); }, [&]()
      {
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
}

static std::atomic_ulong WebServerWatchDogStartTime(0);
void WebServerWatchDogloop(void *param)
{
  WebServer *server = (WebServer *)param;
  WebServerWatchDogStartTime.store(millis());
  while (true)
  {
    if (millis() > (WebServerWatchDogStartTime.load() + 30000))
    {
      server->stop();
      server->begin();
      const char *errMsg = "Error: WebServerWatchdog resart server";
      DEBUG_PRINTLN(errMsg);
      LOG_TO_FILE_E(errMsg);
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
  server->begin();
  while (true)
  {
    try
    {
      server->handleClient();
      WebServerWatchDogFeed();
      if (restart)
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
      const char *errMsg = "Error Caught Exception %s";
      DEBUG_PRINTF(errMsg, e.what());
      LOG_TO_FILE_E(errMsg, e.what());
    }
    catch (...)
    {
      restart = true;
      const char *errMsg = "Error Unhandled exception trapped in webserver loop";
      DEBUG_PRINTLN(errMsg);
      LOG_TO_FILE_E(errMsg);
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
      3072,                    /* Stack size in words */
      (void *)&server,         /* Task input parameter */
      10,                      /* Priority of the task */
      NULL,                    /* Task handle. */
      1);
}
#endif /*ENABLE_OTA_WEBSERVER*/
