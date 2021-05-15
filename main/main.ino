#include "main.h"

#include <sstream>
#include <iomanip>
#include "WiFiManager.h"

#include "config.h"

#if USE_MESH
#include <painlessMesh.h>
#include "mesh_network.h"
#else
#include <TaskScheduler.h>
#endif

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

void mainTaskCallback();
void publishAvailability(void);

MyRWMutex trackedDevicesMutex;
std::vector<BLETrackedDevice> BLETrackedDevices;
std::map<std::string, bool> FastDiscovery;
Task mainTask(TASK_IMMEDIATE, TASK_FOREVER, &mainTaskCallback);
Task mqttAvailabilityTask(5000, TASK_FOREVER, &publishAvailability); //Publish every 5 seconds
#define ScheduleMainTask()            \
  scheduler::get().addTask(mainTask); \
  mainTask.enable();
#define ScheduleMqttAvailabilityTask()            \
  scheduler::get().addTask(mqttAvailabilityTask); \
  mqttAvailabilityTask.enable();

#define SYS_INFORMATION_DELAY 120000 /*2 minutes*/
unsigned long lastSySInfoTime = 0;

#if ENABLE_OTA_WEBSERVER
void webservertaskCallback();
Task webserverTask(TASK_IMMEDIATE, TASK_FOREVER, &webservertaskCallback);
#define ScheduleWebServerTask()            \
  scheduler::get().addTask(webserverTask); \
  webserverTask.enable();
#endif

#if USE_MESH
#if MESH_BRIDGE_NODE
void publishNodeIdCallback();
Task publishNodeIdTask(30000, TASK_FOREVER, &publishNodeIdCallback); //Notify every 30 seconds
#define SchedulePublishNodeIdTask()            \
  scheduler::get().addTask(publishNodeIdTask); \
  publishNodeIdTask.enable();
#endif
void meshUpdateCallback();
Task meshUpdateTask(TASK_IMMEDIATE, TASK_FOREVER, &meshUpdateCallback);
#define ScheduleMeshUpdateTask()            \
  scheduler::get().addTask(meshUpdateTask); \
  meshUpdateTask.enable();
#define NetworkAvailable meshNetwork::hasIP()
#else
#define NetworkAvailable true
#endif

#ifndef ScheduleWebServerTask
#define ScheduleWebServerTask()
#endif

#ifndef ScheduleMeshUpdateTask
#define ScheduleMeshUpdateTask()
#endif

#ifndef SchedulePublishNodeIdTask
#define SchedulePublishNodeIdTask()
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

void publishAvailability(void)
{
#if USE_MQTT
  if (NetworkAvailable)
    publishAvailabilityToMQTT();
#endif
}
#if USE_MESH
void publishNodeIdCallback()
{
  meshNetwork::publishNodeId();
}

void meshUpdateCallback()
{
  meshNetwork::meshUpdate();
}
#endif

#if ENABLE_OTA_WEBSERVER
void webservertaskCallback()
{
  DEBUG_PRINTLN("Executing webservertaskCallback");
  for (int i = 0; i < 100; i++)
    webserver.handleClient();
}
#endif

void publishDevicesInfoCallback()
{
  DEBUG_PRINTLN("Executing publishDevicesInfoCallback");
  if (BLETask::ScanCompleted())
  {
#if USE_MQTT
    if (NetworkAvailable)
    {
      publishAvailabilityToMQTT();
      uint32_t nodeId = 0;
#if USE_MESH
      nodeId = meshNetwork::getNodeId();
#endif
      for (auto &trackedDevice : BLETrackedDevices)
      {
        if (trackedDevice.isDiscovered)
        {
          publishBLEState(trackedDevice.address, MQTT_PAYLOAD_ON, trackedDevice.rssiValue, trackedDevice.batteryLevel, nodeId);
        }
        else
        {
          publishBLEState(trackedDevice.address, MQTT_PAYLOAD_OFF, -100, trackedDevice.batteryLevel, nodeId);
        }
      }

      //System Information
      if (((lastSySInfoTime + SYS_INFORMATION_DELAY) < NTPTime::seconds()) || (lastSySInfoTime == 0))
      {
        publishSySInfo();
        lastSySInfoTime = NTPTime::seconds();
      }

      //publishAvailabilityToMQTT();
    }

#elif USE_MESH
    meshNetwork::publish();
#elif USE_FHEM_LEPRESENCE_SERVER
    FHEMLePresenceServer::loop(); //Handle clients connections
#endif
  }
  Watchdog::Feed();
  mainTask.setCallback(&mainTaskCallback);
}

void ReadBatteryForDecivesCallback()
{
  #if !(USE_MESH && MESH_BRIDGE_NODE)
  DEBUG_PRINTLN("Executing ReadBatteryForDecivesCallback");
  BLETask::ReadBatteryForDevices(publishAvailability, true);
  Watchdog::Feed();
  #endif
  mainTask.setCallback(&publishDevicesInfoCallback);
}

void mainTaskUpdateDeviceStateCallback()
{
  DEBUG_PRINTLN("Executing mainTaskUpdateDeviceStateCallback");
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
  mainTask.setCallback(&ReadBatteryForDecivesCallback);
#else
  mainTask.setCallback(&publishDevicesInfoCallback);
#endif
}

void mainTaskCallback()
{
  DEBUG_PRINTLN("Executing mainTaskCallback");
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

#if !(USE_MESH && MESH_BRIDGE_NODE)
  BLETask::Scan();
#endif

  Watchdog::Feed();
  mainTask.setCallback(&mainTaskUpdateDeviceStateCallback);
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

#if USE_MESH
  meshNetwork::setup();
#else
  WiFiConnect(WIFI_SSID, WIFI_PASSWORD);
#endif

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

  if (NetworkAvailable)
    connectToMQTT();
#endif

#if USE_FHEM_LEPRESENCE_SERVER
  FHEMLePresenceServer::initializeServer();
#endif

  ////////////TASK SCHEDULING////////////
  ScheduleMeshUpdateTask();
  ScheduleWebServerTask();
  ScheduleMainTask();
  ScheduleMeshUpdateTask();
  ScheduleWebServerTask();
  SchedulePublishNodeIdTask();
  ScheduleMeshUpdateTask();
  ScheduleWebServerTask();
  ScheduleMqttAvailabilityTask();
  ScheduleMeshUpdateTask();
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
  if (NetworkAvailable)
  {
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
  }
  else
  {
#if ENABLE_OTA_WEBSERVER
    webserverTask.disable();
#endif
#if USE_MQTT
    mqttAvailabilityTask.disable();
#endif
#if USE_MESH && MESH_BRIDGE_NODE
    publishNodeIdTask.disable();
#endif
    mainTask.disable();
  }

  scheduler::get().execute();

#if USE_MQTT
  mqttLoop();
#endif

  Watchdog::Feed();
}
