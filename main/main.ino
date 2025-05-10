#include "main.h"

#include <BLEDevice.h>
#include <sstream>
#include <iomanip>
#include "WiFiManager.h"
#include "BLEScanTask.h"

#include "config.h"

#define CONFIG_ESP32_DEBUG_OCDAWARE 1

#include "esp_system.h"
#include "DebugPrint.h"

#if ENABLE_OTA_WEBSERVER
#include "OTAWebServer.h"
#endif

#include "settings.h"
#include "watchdog.h"

#include "SPIFFSLogger.h"
#include "utility.h"

#if USE_MQTT
#include "mqtt_client.h"
#endif

#if USE_UDP
#include "udp_client.h"
#endif

#if USE_FHEM_LEPRESENCE_SERVER
#include "fhem_lepresence_server.h"
#endif

#include "NTPTime.h"

//MyRWMutex trackedDevicesMutex;
//std::vector<BLETrackedDevice> BLETrackedDevices;
//std::map<std::string, bool> FastDiscovery;

//BLEScan *pBLEScan;

#define SYS_INFORMATION_DELAY 120 /*2 minutes*/
unsigned long lastSySInfoTime = 0;

#if ENABLE_OTA_WEBSERVER
OTAWebServer webserver;
#endif

extern "C"
{
  void vApplicationMallocFailedHook(void);
  void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);
}

void vApplicationMallocFailedHook(void)
{
  DEBUG_PRINTLN("---MallocFailed----");
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  DEBUG_PRINTF("StackOverflow:%x (%s)\n", xTask, pcTaskName);
}

///////////////////////////////////////////////////////////////////////////
//   BLUETOOTH
///////////////////////////////////////////////////////////////////////////




void LogResetReason()
{
  esp_reset_reason_t r = esp_reset_reason();
  char *msg;
  switch (r)
  {
  case ESP_RST_POWERON:
    msg = "Reset due to power-on event";
    break;
  case ESP_RST_EXT:
    msg = "Reset by external pin";
    break;
  case ESP_RST_SW:
    msg = " Software reset via esp_restart";
    break;
  case ESP_RST_PANIC:
    msg = "Software reset due to exception/panic";
    break;
  case ESP_RST_INT_WDT:
    msg = "Reset (software or hardware) due to interrupt watchdog";
    break;
  case ESP_RST_TASK_WDT:
    msg = "Reset due to task watchdog";
    break;
  case ESP_RST_WDT:
    msg = "Reset due to other watchdogs";
    break;
  case ESP_RST_DEEPSLEEP:
    msg = "Reset after exiting deep sleep mode";
    break;
  case ESP_RST_BROWNOUT:
    msg = "Brownout reset (software or hardware)";
    break;
  case ESP_RST_SDIO:
    msg = "Reset over SDIO";
    break;
  case ESP_RST_UNKNOWN:
  default:
    msg = "Reset reason can not be determined";
    break;
  }
  DEBUG_PRINTLN(msg);
  LOG_TO_FILE_E(msg);
}

///////////////////////////////////////////////////////////////////////////
//   SETUP() & LOOP()
///////////////////////////////////////////////////////////////////////////
void setup()
{
#if defined(DEBUG_SERIAL)
  Serial.begin(115200);
  while(!Serial) ;
#endif

  Serial.println("INFO: Running setup");

#if defined(_SPIFFS_H_)
  if (!SPIFFS.begin())
  {
    if (!SPIFFS.format())
      DEBUG_PRINTLN("Failed to initialize SPIFFS");
  }
#endif

#if ERASE_DATA_AFTER_FLASH
  int dataErased = 0; // 0 -> Data erased not performed
  String ver = Firmware::FullVersion();
  if (ver != Firmware::readVersion())
  {
    dataErased++; // 1 -> Data should be erased
    if (SPIFFS.format())
      dataErased++; // 2 -> Data erased
    Firmware::writeVersion();
  }
#endif // ERASE_DATA_AFTER_FLASH

  const char *settingsFile = "/settings.bin";
  SettingsMngr.SettingsFile(settingsFile);
  if (SPIFFS.exists(settingsFile))
    SettingsMngr.Load();

#if ENABLE_FILE_LOG
  SPIFFSLogger.Initialize("/logs.bin", MAX_NUM_OF_SAVED_LOGS);
  SPIFFSLogger.setLogLevel(SPIFFSLoggerClass::LogLevel(SettingsMngr.logLevel));
#endif

#if ENABLE_OTA_WEBSERVER
  webserver.setup(SettingsMngr.gateway);
#endif

  if (SettingsMngr.wifiSSID.isEmpty())
  {
    WiFiManager::StartAccessPointMode();
  }
  else
  {
    WiFiManager::WiFiConnect(SettingsMngr.wifiSSID, SettingsMngr.wifiPwd);
  }

  LogResetReason();

#if ERASE_DATA_AFTER_FLASH
  if (dataErased == 1)
    LOG_TO_FILE_E("Error Erasing all persitent data!");
  else if (dataErased == 2)
    LOG_TO_FILE_I("Erased all persitent data!");
#endif

#if ENABLE_OTA_WEBSERVER
  webserver.begin();
#endif

  Watchdog::Initialize();

  BLEScanTask::InitializeBLEScan();
  

  if (!WiFiManager::IsAccessPointModeOn())
  {

#if USE_MQTT
    MQTTClient::initializeMQTT();
    MQTTClient::connectToMQTT();
    #if ENABLE_HOME_ASSISTANT_MQTT_DISCOVERY
    if ( MQTTClient::connectToMQTT())
    {
      MQTTClient::publishTrackerDeviceDiscovery();
      MQTTClient::publishDevicesListSensorDiscovery();
      
      MQTTClient::publishTrackerStatus();
      MQTTClient::publishDevicesList();
    }
  #endif // ENABLE_HOME_ASSISTANT_MQTT_DISCOVERY
#endif

#if USE_UDP
    UDPClient::initializeUDP();
#endif

#if USE_FHEM_LEPRESENCE_SERVER
    FHEMLePresenceServer::initializeServer();
#endif
  }

  LOG_TO_FILE("BLETracker initialized");
}



#if USE_MQTT
std::function<void(const BLETrackedDevice &)> publishBLEState= [](const BLETrackedDevice &device) { MQTTClient::publishBLEState(device);};
std::function<void(void)>  publishSySInfo = MQTTClient::publishSySInfo;
#elif USE_UDP
std::function<void(const BLETrackedDevice &)> publishBLEState = UDPClient::publishBLEState;
std::function<void(void)>  publishSySInfo = UDPClient::publishSySInfo;
#endif

void loop()
{
  try
  {
    if (WiFiManager::IsAccessPointModeOn())
    {
      WiFiManager::CheckAPModeTimeout();
    }
#if USE_MQTT
    else
    {
      MQTTClient::mqttLoop();
    }
#endif

#if USE_FHEM_LEPRESENCE_SERVER
    // Check and restore the wifi connection if it's loose
    if (!SettingsMngr.wifiSSID.isEmpty())
    {
      WiFiManager::WiFiConnect(SettingsMngr.wifiSSID, SettingsMngr.wifiPwd);
    }
#endif

    Watchdog::Feed();

   // Serial.println("INFO: Running mainloop");
   // DEBUG_PRINTF("Main loop Free heap: %u\n", xPortGetFreeHeapSize());
   // DEBUG_PRINTF("Number device discovered: %d\n", BLETrackedDevices.size());

    if (BLEScanTask::GetTrackedDevicesCount() == SettingsMngr.GetMaxNumOfTraceableDevices())
    {
      const char* errMsg = "Restarting: reached the max number of traceable devices";
      DEBUG_PRINTLN(errMsg);
      LOG_TO_FILE(errMsg);
      esp_restart();
    }

    if (!WiFiManager::IsAccessPointModeOn())
    {

      BLEScanTask::BLEScanloop();
#if PUBLISH_BATTERY_LEVEL
#if PROGRESSIVE_SCAN
      if (scanCompleted)
#endif
 //       BLEScanTask::batteryTask();
#endif

  bool scanEnabled = BLEScanTask::IsScanEnabled();
#if 0 & (USE_MQTT || USE_UDP)
      bool publishSystemInfo = ((lastSySInfoTime + SYS_INFORMATION_DELAY) < NTPTime::seconds()) || (lastSySInfoTime == 0);

      if (scanEnabled || publishSystemInfo)
      {
        CRITICALSECTION_READSTART(trackedDevicesMutex)
        for (auto &trackedDevice : BLETrackedDevices)
        {
          publishBLEState(trackedDevice);
        }
        CRITICALSECTION_READEND(trackedDevicesMutex)
      }

      // System Information
      if (publishSystemInfo)
      {
        publishSySInfo();
        lastSySInfoTime = NTPTime::seconds();
      }

#elif USE_FHEM_LEPRESENCE_SERVER
      FHEMLePresenceServer::loop(); // Handle clients connections
#endif
    }
  }
  catch (std::exception &e)
  {
    constexpr const char* errMsg = "Error Caught Exception %s";
    DEBUG_PRINTF(errMsg, e.what());
    LOG_TO_FILE_E(errMsg, e.what());
  }
  catch (...)
  {
    const char* errMsg = "Error Unhandled exception trapped in main loop";
    DEBUG_PRINTLN(errMsg);
    LOG_TO_FILE_E(errMsg);
  }

  delay(WiFiManager::IsAccessPointModeOn() ? 300 : 100);
}
