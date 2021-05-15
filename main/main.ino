#include "main.h"

#include <sstream>
#include <iomanip>
#include "WiFiManager.h"

#include "config.h"

#include <TaskScheduler.h>

#define CONFIG_ESP32_DEBUG_OCDAWARE 1

//#include "esp_system.h"
#include "DebugPrint.h"

#if ENABLE_OTA_WEBSERVER
#include "OTAWebServer.h"
#endif

#include "settings.h"
#include "watchdog.h"

#include "SPIFFSLogger.h"
#include "utility.h"

#include "Scheduler.h"

#if USE_MQTT
#include "mqtt_client.h"
#endif

#if USE_FHEM_LEPRESENCE_SERVER
#include "fhem_lepresence_server.h"
#endif

#include "NTPTime.h"

#include "bletasks.h"

void TaskCallback_BleScan();
void TaskCallBack_PublishAvailability(void);

MyRWMutex trackedDevicesMutex;
std::vector<BLETrackedDevice> BLETrackedDevices;
std::map<std::string, bool> FastDiscovery;

//MainTask is splitted in several callback to allow other tasks to be executed
Task mainTask(TASK_IMMEDIATE, TASK_FOREVER, &TaskCallback_BleScan);//<--TaskCallback_BleScan is the entry point of the task chain
Task mqttAvailabilityTask(5000, TASK_FOREVER, &TaskCallBack_PublishAvailability); //Publish every 5 seconds

#define ScheduleMainTask()            \
  scheduler::get().addTask(mainTask); \
  mainTask.enable();

#if USE_MQTT
#define ScheduleMqttAvailabilityTask()            \
  scheduler::get().addTask(mqttAvailabilityTask); \
  mqttAvailabilityTask.enable();
#else
#define ScheduleMqttAvailabilityTask()
#endif

#define SYS_INFORMATION_DELAY 120000 /*2 minutes*/
unsigned long lastSySInfoTime = 0;

#if ENABLE_OTA_WEBSERVER
void TaskCallback_WebServerHandleClient();
Task webserverTask(TASK_IMMEDIATE, TASK_FOREVER, &TaskCallback_WebServerHandleClient);
#define ScheduleWebServerTask()            \
  scheduler::get().addTask(webserverTask); \
  webserverTask.enable();
#else
#define ScheduleWebServerTask()
#endif


#if USE_FHEM_LEPRESENCE_SERVER
void TaskCallback_HandleFHEMClientsConnections();
Task FHEMServerTask(TASK_IMMEDIATE, TASK_FOREVER, &TaskCallback_HandleFHEMClientsConnections);
#define ScheduleFHEMServerTask()            \
  scheduler::get().addTask(FHEMServerTask); \
  FHEMServerTask.enable();
#else
#define ScheduleFHEMServerTask()
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

char *formatMillis(unsigned long milliseconds, char outstr[20])
{
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;
  snprintf(outstr, 20, "%d.%02d:%02d:%02d", days, hours % 24, minutes % 60, seconds % 60);
  return outstr;
}

void TaskCallBack_PublishAvailability(void)
{
#if USE_MQTT
    publishAvailabilityToMQTT();
#endif
}

#if ENABLE_OTA_WEBSERVER
void TaskCallback_WebServerHandleClient()
{
  DEBUG_PRINTLN("Executing TaskCallback_WebServerHandleClient");
  webserver.handleClient();
}
#endif


#if USE_FHEM_LEPRESENCE_SERVER
void TaskCallback_HandleFHEMClientsConnections()
{
  DEBUG_PRINTLN("Executing HandleFHEMClientsConnections");
  if (BLETask::ScanCompleted())
  {
    FHEMLePresenceServer::loop(); //Handle clients connections
  }
}
#endif 

void TaskCallback_PublishDevicesInfo()
{
  DEBUG_PRINTLN("Executing PublishDevicesInfoCallback");
  if (BLETask::ScanCompleted())
  {
#if USE_MQTT
      publishAvailabilityToMQTT();

      for (auto &trackedDevice : BLETrackedDevices)
      {
        if (trackedDevice.isDiscovered)
        {
          publishBLEState(trackedDevice.address, MQTT_PAYLOAD_ON, trackedDevice.rssiValue, trackedDevice.batteryLevel);
        }
        else
        {
          publishBLEState(trackedDevice.address, MQTT_PAYLOAD_OFF, -100, trackedDevice.batteryLevel);
        }
      }

      //System Information
      if (((lastSySInfoTime + SYS_INFORMATION_DELAY) < NTPTime::seconds()) || (lastSySInfoTime == 0))
      {
        publishSySInfo();
        lastSySInfoTime = NTPTime::seconds();
      }
#elif USE_FHEM_LEPRESENCE_SERVER
    FHEMLePresenceServer::loop(); //Handle clients connections
#endif
  }

  Watchdog::Feed();
  mainTask.setCallback(&TaskCallback_BleScan);
}

void TaskCallback_ReadBatteryForDecives()
{
  DEBUG_PRINTLN("Executing TaskCallback_ReadBatteryForDecives");
  BLETask::ReadBatteryForDevices(TaskCallBack_PublishAvailability, true);
  Watchdog::Feed();
  mainTask.setCallback(&TaskCallback_PublishDevicesInfo);
}

void TaskCallback_UpdateDevicesState()
{
  DEBUG_PRINTLN("Executing TaskCallback_UpdateDevicesState");
  for (auto &trackedDevice : BLETrackedDevices)
  {
    if (trackedDevice.isDiscovered && (trackedDevice.lastDiscoveryTime + SettingsMngr.maxNotAdvPeriod) < NTPTime::seconds())
    {
      trackedDevice.isDiscovered = false;
      FastDiscovery[trackedDevice.address] = false;
      LOG_TO_FILE_D("Devices %s is gone out of range", trackedDevice.address);
    }
  }

  Watchdog::Feed();
#if PUBLISH_BATTERY_LEVEL
  mainTask.setCallback(&TaskCallback_ReadBatteryForDecives);
#else
  mainTask.setCallback(&TaskCallback_PublishDevicesInfo);
#endif
}

void TaskCallback_BleScan()
{
  DEBUG_PRINTLN("Executing TaskCallback_BleScan");
#if USE_FHEM_LEPRESENCE_SERVER
  //Check and restore the wifi connection if it's loose
  WiFiConnect(WIFI_SSID, WIFI_PASSWORD);
#endif

  DEBUG_PRINTF("Number device discovered: %d\n", BLETrackedDevices.size());

  if (BLETrackedDevices.size() == SettingsMngr.GetMaxNumOfTraceableDevices())
  {
    DEBUG_PRINTLN("INFO: Restart because the array is full\n");
    LOG_TO_FILE("Restarting: reached the max number of traceable devices");
    esp_restart();
  }

  BLETask::Scan();

  Watchdog::Feed();
  mainTask.setCallback(&TaskCallback_UpdateDevicesState);
}

///////////////////////////////////////////////////////////////////////////
//   SETUP() & LOOP()
///////////////////////////////////////////////////////////////////////////
void setup()
{
#if defined(DEBUG_SERIAL)
  Serial.begin(115200);
#endif

  Serial.println("INFO: Running setup");

  scheduler::get().init();

#if defined(_SPIFFS_H_)
  if (!SPIFFS.begin())
  {
    if (!SPIFFS.format())
      DEBUG_PRINTLN("Failed to initialize SPIFFS");
  }
#endif

#if ERASE_DATA_AFTER_FLASH
  int dataErased = 0; //0 -> Data erased not performed
  String ver = Firmware::FullVersion();
  if (ver != Firmware::readVersion())
  {
    dataErased++; //1 -> Data should be erased
    if (SPIFFS.format())
      dataErased++; //2 -> Data erased
    Firmware::writeVersion();
  }
#endif //ERASE_DATA_AFTER_FLASH

  const char *settingsFile = "/settings.bin";
  SettingsMngr.SettingsFile(settingsFile);
  if (SPIFFS.exists(settingsFile))
    SettingsMngr.Load();

#if ENABLE_FILE_LOG
  SPIFFSLogger.Initialize("/logs.bin", MAX_NUM_OF_SAVED_LOGS);
  SPIFFSLogger.setLogLevel(SPIFFSLoggerClass::LogLevel(SettingsMngr.logLevel));
#endif

  WiFiConnect(WIFI_SSID, WIFI_PASSWORD);

  LogResetReason();

#if ERASE_DATA_AFTER_FLASH
  if (dataErased == 1)
    LOG_TO_FILE_E("Error Erasing all persitent data!");
  else if (dataErased == 2)
    LOG_TO_FILE_I("Erased all persitent data!");
#endif

#if ENABLE_OTA_WEBSERVER
  webserver.setup(GATEWAY_NAME, WIFI_SSID, WIFI_PASSWORD);
#endif

  BLETrackedDevices.reserve(SettingsMngr.GetMaxNumOfTraceableDevices());
  for (const auto &dev : SettingsMngr.GetKnownDevicesList())
  {
    BLETrackedDevice trackedDevice;
    memcpy(trackedDevice.address, dev.address, ADDRESS_STRING_SIZE);
    trackedDevice.forceBatteryRead = dev.readBattery;
    BLETrackedDevices.push_back(std::move(trackedDevice));
  }

  Watchdog::Initialize();

  BLETask::Initialize();

#if USE_MQTT
  initializeMQTT();
  connectToMQTT();
#endif

#if USE_FHEM_LEPRESENCE_SERVER
  FHEMLePresenceServer::initializeServer();
#endif

  ////////////TASK SCHEDULING////////////
  //Order metter
  ScheduleWebServerTask();
  ScheduleMainTask();
  ScheduleWebServerTask();
  ScheduleFHEMServerTask();
  ScheduleWebServerTask();
  ScheduleFHEMServerTask();
  ScheduleMqttAvailabilityTask();
  ScheduleWebServerTask();
  /////////////////////////////////////////

  LOG_TO_FILE("BLETracker initialized");
}

void loop()
{
  /*
    Serial.println("INFO: Running mainloop");
#if USE_MESH && MESH_BRIDGE_NODE
  DEBUG_PRINTLN("INFO: Node configured as Bridge");
#endif
  DEBUG_PRINTF("Main loop Free heap: %u\n", xPortGetFreeHeapSize());
*/

    NTPTime::initialize();
#if ENABLE_OTA_WEBSERVER
    webserverTask.enable();
#endif
#if USE_MQTT
    mqttAvailabilityTask.enable();
#endif
#if USE_MESH && MESH_BRIDGE_NODE
    publishNodeIdTask.enable();
#endif
    mainTask.enable();

  scheduler::get().execute();

#if USE_MQTT
  mqttLoop();
#endif

  Watchdog::Feed();
}
