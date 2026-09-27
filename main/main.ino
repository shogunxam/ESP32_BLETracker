#include "main.h"

#include <BLEDevice.h>
#include "BleDeviceId.h"
#include <sstream>
#include <iomanip>
#include "WiFiManager.h"

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

#include "AdvertisedDeviceCallbacks.h"

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

MyRWMutex trackedDevicesMutex;
std::vector<BLETrackedDevice> BLETrackedDevices;
std::map<BleDeviceId, bool> FastDiscovery;
bool discoveryMode = false;

BLEScan *pBLEScan;

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


static BLEUUID service_BATT_UUID(BLEUUID((uint16_t)0x180F));
static BLEUUID char_BATT_UUID(BLEUUID((uint16_t)0x2A19));

#if PUBLISH_BATTERY_LEVEL
class MyBLEClientCallBack : public BLEClientCallbacks
{
  void onConnect(BLEClient *pClient)
  {
  }

  virtual void onDisconnect(BLEClient *pClient)
  {
    log_i(" >> onDisconnect callback");
    pClient->disconnect();
  }
};

void ForceBatteryRead(const BleDeviceId& deviceId)
{
  for (auto &trackedDevice : BLETrackedDevices)
  {
    if (trackedDevice.deviceId == deviceId)
    {
      trackedDevice.forceBatteryRead = true;
      return;
    }
  }
}

void batteryTask()
{
  if (discoveryMode)
    return;
  // DEBUG_PRINTF("\n*** Memory before battery scan: %u\n",xPortGetFreeHeapSize());

  char deviceIdAsString[BleDeviceId::UUID_STRING_SIZE];
  for (auto &trackedDevice : BLETrackedDevices)
  {
    if (!(SettingsMngr.InBatteryList(trackedDevice.deviceId) || trackedDevice.forceBatteryRead))
      continue;

    trackedDevice.deviceId.toString(deviceIdAsString);
    // We need to connect to the device to read the battery value
    // So that we check only the device really advertised by the scan
    unsigned long BatteryReadTimeout = trackedDevice.lastBattMeasureTime + BATTERY_READ_PERIOD;
    unsigned long BatteryRetryTimeout = trackedDevice.lastBattMeasureTime + BATTERY_RETRY_PERIOD;
    unsigned long now = NTPTime::getTimeStamp();
    bool batterySet = trackedDevice.batteryLevel > 0;
    if (trackedDevice.advertised && trackedDevice.hasBatteryService && trackedDevice.rssiValue > -90 &&
        ((batterySet && (BatteryReadTimeout < now)) ||
         (!batterySet && (BatteryRetryTimeout < now)) ||
         trackedDevice.forceBatteryRead))
    {
      trackedDevice.deviceId.toString(deviceIdAsString);
      DEBUG_PRINTF("\nReading Battery level for %s: Retries: %d\n", deviceIdAsString, trackedDevice.connectionRetry);
      bool connectionEstablished = batteryLevel(trackedDevice.deviceId, trackedDevice.addressType, trackedDevice.batteryLevel, trackedDevice.hasBatteryService);
      if (connectionEstablished || !trackedDevice.hasBatteryService)
      {
        log_i("Device %s has battery service: %s", deviceIdAsString, trackedDevice.hasBatteryService ? "YES" : "NO");
        trackedDevice.connectionRetry = 0;
        trackedDevice.lastBattMeasureTime = now;
      }
      else
      {
        trackedDevice.connectionRetry++;
        if (trackedDevice.connectionRetry >= MAX_BLE_CONNECTION_RETRIES)
        {
          trackedDevice.connectionRetry = 0;
          trackedDevice.lastBattMeasureTime = now;
          // Report the error only one time if Log level info is set
          LOG_TO_FILE_E("Error: Connection to device %s failed", deviceIdAsString);
          DEBUG_PRINTF("Error: Connection to device %s failed", deviceIdAsString);
        }
        else
        {
          // Report the error every time if Log level debug or verbose is set
          LOG_TO_FILE_D("Error: Connection to device %s failed", deviceIdAsString);
          DEBUG_PRINTF("Error: Connection to device %s failed", deviceIdAsString);
        }
      }
    }
    else if (BatteryReadTimeout < now)
    {
      // Here we preserve the lastBattMeasure time to trigger a new read
      // when the device will be advertised again
      trackedDevice.batteryLevel = -1;
    }
    trackedDevice.forceBatteryRead = false;
    Watchdog::Feed();
  }
  // DEBUG_PRINTF("\n*** Memory after battery scan: %u\n",xPortGetFreeHeapSize());
}

bool batteryLevel(const BleDeviceId &deviceId, esp_ble_addr_type_t addressType, int8_t &battLevel, bool &hasBatteryService)
{
  log_i(">> ------------------batteryLevel----------------- ");
  bool bleconnected;
  BLEClient client;
  battLevel = -1;

  if(deviceId.isIBeacon())
  {
    hasBatteryService = false;
    return false;
  }

  char deviceIdAsString[BleDeviceId::UUID_STRING_SIZE];
  deviceId.toString(deviceIdAsString);

  esp_bd_addr_t rawAddress; 
  memcpy(rawAddress, deviceId.getRawID(), sizeof(esp_bd_addr_t));

  BLEAddress bleAddress = BLEAddress(rawAddress); 
  log_i("connecting to : %s", bleAddress.toString().c_str());
  LOG_TO_FILE_D("Reading battery level for device %s", deviceIdAsString);
  MyBLEClientCallBack callback;
  client.setClientCallbacks(&callback);

  // Connect to the remote BLE Server.
  bleconnected = client.connect(bleAddress, addressType);
  if (bleconnected)
  {
    log_i("Connected to server");
    BLERemoteService *pRemote_BATT_Service = client.getService(service_BATT_UUID);
    if (pRemote_BATT_Service == nullptr)
    {
      log_i("Cannot find the BATTERY service.");
      LOG_TO_FILE_E("Error: Cannot find the BATTERY service for the device %s", deviceIdAsString);
      hasBatteryService = false;
    }
    else
    {
      BLERemoteCharacteristic *pRemote_BATT_Characteristic = pRemote_BATT_Service->getCharacteristic(char_BATT_UUID);
      if (pRemote_BATT_Characteristic == nullptr)
      {
        log_i("Cannot find the BATTERY characteristic.");
        LOG_TO_FILE_E("Error: Cannot find the BATTERY characteristic for device %s", deviceIdAsString);
        hasBatteryService = false;
      }
      else
      {
        std::string value = pRemote_BATT_Characteristic->readValue();
        if (value.length() > 0)
          battLevel = (int8_t)value[0];
        log_i("Reading BATTERY level : %d", battLevel);
        LOG_TO_FILE_I("Battery level for device %s is %d", deviceIdAsString, battLevel);
        hasBatteryService = true;
      }
    }
    // Before disconnecting I need to pause the task to wait (I don't know what), otherwise we have an heap corruption
    // delay(200);
    if (client.isConnected())
    {
      log_i("disconnecting...");
      client.disconnect();
    }
    log_i("waiting for disconnection...");
    while (client.isConnected())
      delay(100);
    log_i("Client disconnected.");
  }
  else
  {
    // We fail to connect and we have to be sure the PeerDevice is removed before delete it
    BLEDevice::removePeerDevice(client.m_appId, true);
    log_i("-------------------Not connected!!!--------------------");
  }

  log_i("<< ------------------batteryLevel----------------- ");
  return bleconnected;
}
#endif

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

  BLETrackedDevices.reserve(discoveryMode ? 90 : SettingsMngr.GetMaxNumOfTraceableDevices());
  for (const auto &dev : SettingsMngr.GetKnownDevicesList())
  {
    BLETrackedDevice trackedDevice;
    trackedDevice.deviceId = dev.deviceId;
    trackedDevice.forceBatteryRead = dev.readBattery;
    BLETrackedDevices.push_back(std::move(trackedDevice));
  }

  Watchdog::Initialize();

  if (!WiFiManager::IsAccessPointModeOn())
  {
    BLEDevice::init(SettingsMngr.gateway.c_str());
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), NUM_OF_ADVERTISEMENT_IN_SCAN > 1);
    pBLEScan->setActiveScan(ACTIVE_SCAN);
    pBLEScan->setInterval(50);
    pBLEScan->setWindow(50);

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

char *formatMillis(unsigned long milliseconds, char outstr[20])
{
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;
  snprintf(outstr, 20, "%d.%02d:%02d:%02d", days, hours % 24, minutes % 60, seconds % 60);
  return outstr;
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

    Serial.println("INFO: Running mainloop");
    DEBUG_PRINTF("Main loop Free heap: %u\n", xPortGetFreeHeapSize());
    DEBUG_PRINTF("Number device discovered: %d\n", BLETrackedDevices.size());

    if (!discoveryMode && BLETrackedDevices.size() == SettingsMngr.GetMaxNumOfTraceableDevices())
    {
      const char* errMsg = "Restarting: reached the max number of traceable devices";
      DEBUG_PRINTLN(errMsg);
      LOG_TO_FILE(errMsg);
      esp_restart();
    }

    if (!WiFiManager::IsAccessPointModeOn())
    {
#if PROGRESSIVE_SCAN
      bool scanCompleted = false;
#endif
      bool scanEnabled = SettingsMngr.IsManualScanOn() || !SettingsMngr.IsManualScanEnabled();

      if (scanEnabled)
      {
#if PROGRESSIVE_SCAN
        static uint32_t elapsedScanTime = 0;
        static uint32_t lastScanTime = 0;

        bool continuePrevScan = elapsedScanTime > 0;
        if (!continuePrevScan) // new scan
        {
          // Reset the states of discovered devices
          for (auto &trackedDevice : BLETrackedDevices)
          {
            trackedDevice.advertised = false;
            trackedDevice.rssiValue = -100;
            trackedDevice.advertisementCounter = 0;
          }
        }

        lastScanTime = NTPTime::seconds();
        pBLEScan->start(1, continuePrevScan);
        pBLEScan->stop();
        elapsedScanTime += NTPTime::seconds() - lastScanTime;
        scanCompleted = elapsedScanTime > SettingsMngr.scanPeriod;
        if (scanCompleted)
        {
          elapsedScanTime = 0;
          pBLEScan->clearResults();
        }
#else
        // Reset the states of discovered devices
        for (auto &trackedDevice : BLETrackedDevices)
        {
          trackedDevice.advertised = false;
          trackedDevice.rssiValue = -100;
          trackedDevice.advertisementCounter = 0;
        }

        // DEBUG_PRINTF("\n*** Memory Before scan: %u\n",xPortGetFreeHeapSize());
        pBLEScan->start(SettingsMngr.scanPeriod);
        pBLEScan->stop();
        pBLEScan->clearResults();
        // DEBUG_PRINTF("\n*** Memory After scan: %u\n",xPortGetFreeHeapSize());
#endif
      }
      else
      {
        for (auto &trackedDevice : BLETrackedDevices)
        {
          trackedDevice.advertised = false;
          trackedDevice.rssiValue = -100;
          trackedDevice.advertisementCounter = 0;
        }
        pBLEScan->clearResults();
      }


      char deviceIdAsString[BleDeviceId::UUID_STRING_SIZE];
      for (auto &trackedDevice : BLETrackedDevices)
      {
        if (trackedDevice.isDiscovered && (trackedDevice.lastDiscoveryTime + SettingsMngr.maxNotAdvPeriod) < NTPTime::seconds())
        {
          trackedDevice.isDiscovered = false;
          FastDiscovery[trackedDevice.deviceId] = false;
          trackedDevice.deviceId.toString(deviceIdAsString);
          LOG_TO_FILE_D("Devices %s is gone out of range", deviceIdAsString);
        }
      }

#if PUBLISH_BATTERY_LEVEL
#if PROGRESSIVE_SCAN
      if (scanCompleted)
#endif
  if (!discoveryMode)
    batteryTask();
#endif

#if USE_MQTT || USE_UDP
      bool publishSystemInfo = ((lastSySInfoTime + SYS_INFORMATION_DELAY) < NTPTime::seconds()) || (lastSySInfoTime == 0);

      if (!discoveryMode && (scanEnabled || publishSystemInfo))
      {
        for (auto &trackedDevice : BLETrackedDevices)
        {
          publishBLEState(trackedDevice);
        }
      }

      // System Information
      if (!discoveryMode && publishSystemInfo)
      {
        publishSySInfo();
        lastSySInfoTime = NTPTime::seconds();
      }

#elif USE_FHEM_LEPRESENCE_SERVER
  if (!discoveryMode)
    FHEMLePresenceServer::loop(); // Handle clients connections
#endif
    }
  }
  catch (std::exception &e)
  {
    const char* errMsg = "Error Caught Exception %s";
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
