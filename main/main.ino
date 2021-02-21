#include "main.h"

#include <BLEDevice.h>
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

#if USE_MQTT
#include "mqtt_client.h"
#endif

#if USE_FHEM_LEPRESENCE_SERVER
#include "fhem_lepresence_server.h"
#endif

#include "NTPTime.h"

MyRWMutex trackedDevicesMutex;
std::vector<BLETrackedDevice> BLETrackedDevices;
std::map<std::string, bool> FastDiscovery;

BLEScan *pBLEScan;

#define SYS_INFORMATION_DELAY 120000 /*2 minutes*/
unsigned long lastSySInfoTime = 0;

#if ENABLE_OTA_WEBSERVER
OTAWebServer webserver;
#endif

///////////////////////////////////////////////////////////////////////////
//   BLUETOOTH
///////////////////////////////////////////////////////////////////////////
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{

  void onResult(BLEAdvertisedDevice advertisedDevice) override
  {
    Watchdog::Feed();
    const uint8_t shortNameSize = 31;
    char address[ADDRESS_STRING_SIZE];
    NormalizeAddress(*(advertisedDevice.getAddress().getNative()), address);

    if (!SettingsMngr.IsTraceable(address))
      return;

    char shortName[shortNameSize];
    memset(shortName, 0, shortNameSize);
    if (advertisedDevice.haveName())
      strncpy(shortName, advertisedDevice.getName().c_str(), shortNameSize - 1);

    int RSSI = advertisedDevice.getRSSI();
    
    CRITICALSECTION_WRITESTART(trackedDevicesMutex)
    for (auto &trackedDevice : BLETrackedDevices)
    {
      if (strcmp(address, trackedDevice.address) == 0)
      {
    #if NUM_OF_ADVERTISEMENT_IN_SCAN > 1
        trackedDevice.advertisementCounter++;
        //To proceed we have to find at least NUM_OF_ADVERTISEMENT_IN_SCAN duplicates during the scan
        //and the code have to be executed only once
        if (trackedDevice.advertisementCounter != NUM_OF_ADVERTISEMENT_IN_SCAN)
          return;
    #endif

        if (!trackedDevice.advertised) //Skip advertised dups
        {
          trackedDevice.addressType = advertisedDevice.getAddressType();
          trackedDevice.advertised = true;
          trackedDevice.lastDiscoveryTime = NTPTime::seconds();
          trackedDevice.rssiValue = RSSI;
          if (!trackedDevice.isDiscovered)
          {
            trackedDevice.isDiscovered = true;
            trackedDevice.connectionRetry = 0;
            FastDiscovery[trackedDevice.address]= true;
            DEBUG_PRINTF("INFO: Tracked device discovered again, Address: %s , RSSI: %d\n", address, RSSI);
            if (advertisedDevice.haveName())
            {
              LOG_TO_FILE_D("Device %s ( %s ) within range, RSSI: %d ", address, shortName, RSSI);
            }
            else
              LOG_TO_FILE_D("Device %s within range, RSSI: %d ", address, RSSI);
          }
          else
          {
            DEBUG_PRINTF("INFO: Tracked device discovered, Address: %s , RSSI: %d\n", address, RSSI);
          }
        }
        return;
      }
    }

    //This is a new device...
    BLETrackedDevice trackedDevice;
    trackedDevice.advertised = NUM_OF_ADVERTISEMENT_IN_SCAN <= 1; //Skip duplicates
    memcpy(trackedDevice.address, address, ADDRESS_STRING_SIZE);
    trackedDevice.addressType = advertisedDevice.getAddressType();
    trackedDevice.isDiscovered = NUM_OF_ADVERTISEMENT_IN_SCAN <= 1;
    trackedDevice.lastDiscoveryTime = NTPTime::seconds();
    trackedDevice.lastBattMeasureTime = 0;
    trackedDevice.batteryLevel = -1;
    trackedDevice.hasBatteryService = true;
    trackedDevice.connectionRetry = 0;
    trackedDevice.rssiValue = RSSI;
    trackedDevice.advertisementCounter = 1;
    BLETrackedDevices.push_back(std::move(trackedDevice));
    FastDiscovery[trackedDevice.address]= true;
  #if NUM_OF_ADVERTISEMENT_IN_SCAN > 1
    //To proceed we have to find at least NUM_OF_ADVERTISEMENT_IN_SCAN duplicates during the scan
    //and the code have to be executed only once
    return;
  #endif
    CRITICALSECTION_WRITEEND;

    DEBUG_PRINTF("INFO: Device discovered, Address: %s , RSSI: %d\n", address, RSSI);
    if (advertisedDevice.haveName())
      LOG_TO_FILE_D("Discovered new device %s ( %s ) within range, RSSI: %d ", address, shortName, RSSI);
    else
      LOG_TO_FILE_D("Discovered new device %s within range, RSSI: %d ", address, RSSI);
  }
};

static BLEUUID service_BATT_UUID(BLEUUID((uint16_t)0x180F));
static BLEUUID char_BATT_UUID(BLEUUID((uint16_t)0x2A19));

#if PUBLISH_BATTERY_LEVEL
//static EventGroupHandle_t connection_event_group;
//const int CONNECTED_EVENT = BIT0;
//const int DISCONNECTED_EVENT = BIT1;

class MyBLEClientCallBack : public BLEClientCallbacks
{
  void onConnect(BLEClient *pClient)
  {
  }

  virtual void onDisconnect(BLEClient *pClient)
  {
    log_i(" >> onDisconnect callback");
    pClient->disconnect();
    //log_i(" >> onDisconnect callback");
    //xEventGroupSetBits(connection_event_group, DISCONNECTED_EVENT);
    //log_i(" << onDisconnect callback");
  }
};

void batteryTask()
{
  //DEBUG_PRINTF("\n*** Memory before battery scan: %u\n",xPortGetFreeHeapSize());

  for (auto &trackedDevice : BLETrackedDevices)
  {
    if (!SettingsMngr.InBatteryList(trackedDevice.address))
      continue;

#if USE_MQTT
    publishAvailabilityToMQTT();
#endif

    //We need to connect to the device to read the battery value
    //So that we check only the device really advertised by the scan
    unsigned long BatteryReadTimeout = trackedDevice.lastBattMeasureTime + BATTERY_READ_PERIOD;
    unsigned long BatteryRetryTimeout = trackedDevice.lastBattMeasureTime + BATTERY_RETRY_PERIOD;
    unsigned long now = NTPTime::seconds();
    bool batterySet = trackedDevice.batteryLevel > 0;
    if (trackedDevice.advertised && trackedDevice.hasBatteryService && trackedDevice.rssiValue > -90 &&
        ((batterySet && (BatteryReadTimeout < now)) ||
         (!batterySet && (BatteryRetryTimeout < now)) ||
         (trackedDevice.lastBattMeasureTime == 0)))
    {
      DEBUG_PRINTF("\nReading Battery level for %s: Retries: %d\n", trackedDevice.address, trackedDevice.connectionRetry);
      bool connectionExtabilished = batteryLevel(trackedDevice.address, trackedDevice.addressType, trackedDevice.batteryLevel, trackedDevice.hasBatteryService);
      if (connectionExtabilished || !trackedDevice.hasBatteryService)
      {
        log_i("Device %s has battery service: %s", trackedDevice.address, trackedDevice.hasBatteryService ? "YES" : "NO");
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
        }
      }
    }
    else if (BatteryReadTimeout < now)
    {
      //Here we preserve the lastBattMeasure time to trigger a new read
      //when the device will be advertised again
      trackedDevice.batteryLevel = -1;
    }
    Watchdog::Feed();
  }
  //DEBUG_PRINTF("\n*** Memory after battery scan: %u\n",xPortGetFreeHeapSize());
}

bool batteryLevel(const char address[ADDRESS_STRING_SIZE], esp_ble_addr_type_t addressType, int8_t &battLevel, bool &hasBatteryService)
{
  log_i(">> ------------------batteryLevel----------------- ");
  bool bleconnected;
  BLEClient client;
  battLevel = -1;
  static char canonicalAddress[ADDRESS_STRING_SIZE + 5];
  CanonicalAddress(address, canonicalAddress);
  BLEAddress bleAddress = BLEAddress(canonicalAddress);
  log_i("connecting to : %s", bleAddress.toString().c_str());
  LOG_TO_FILE_D("Reading battery level for device %s", address);
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
      LOG_TO_FILE_E("Error: Cannot find the BATTERY service for the device %s", address);
      hasBatteryService = false;
    }
    else
    {
      BLERemoteCharacteristic *pRemote_BATT_Characteristic = pRemote_BATT_Service->getCharacteristic(char_BATT_UUID);
      if (pRemote_BATT_Characteristic == nullptr)
      {
        log_i("Cannot find the BATTERY characteristic.");
        LOG_TO_FILE_E("Error: Cannot find the BATTERY characteristic for device %s", address);
        hasBatteryService = false;
      }
      else
      {
        std::string value = pRemote_BATT_Characteristic->readValue();
        if (value.length() > 0)
          battLevel = (int8_t)value[0];
        log_i("Reading BATTERY level : %d", battLevel);
        LOG_TO_FILE_I("Battery level for device %s is %d", address, battLevel);
        hasBatteryService = true;
      }
    }
    //Before disconnecting I need to pause the task to wait (I'don't know what), otherwhise we have an heap corruption
    //delay(200);
    if (client.isConnected())
    {
      log_i("disconnecting...");
      client.disconnect();
    }
    log_i("waiting for disconnection...");
    while (client.isConnected())
      delay(100);
    log_i("Client disconnected.");
    //EventBits_t bits = xEventGroupWaitBits(connection_event_group, DISCONNECTED_EVENT, true, true, portMAX_DELAY);
    //log_i("wait for disconnection done: %d", bits);
  }
  else
  {
    //We fail to connect and we have to be sure the PeerDevice is removed before delete it
    BLEDevice::removePeerDevice(client.m_appId, true);
    log_i("-------------------Not connected!!!--------------------");
    LOG_TO_FILE_E("Error: Connection to device %s failed", address);
    DEBUG_PRINTF("Error: Connection to device %s failed", address);
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
  webserver.begin();
#endif

  BLETrackedDevices.reserve(SettingsMngr.GetMaxNumOfTraceableDevices());

  Watchdog::Initialize();

  BLEDevice::init(GATEWAY_NAME);
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), NUM_OF_ADVERTISEMENT_IN_SCAN > 1);
  pBLEScan->setActiveScan(ACTIVE_SCAN);
  pBLEScan->setInterval(50);
  pBLEScan->setWindow(50);

#if USE_MQTT
  initializeMQTT();
  connectToMQTT();
#endif

#if USE_FHEM_LEPRESENCE_SERVER
  FHEMLePresenceServer::Start();
#endif

#if PUBLISH_BATTERY_LEVEL
  //connection_event_group = xEventGroupCreate();
#endif

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

void loop()
{
  try
  {
#if USE_MQTT
    mqttLoop();
#endif

#if USE_FHEM_LEPRESENCE_SERVER
  //Check and restore the wifi connection if it's loose
  WiFiConnect(WIFI_SSID, WIFI_PASSWORD);
#endif

    Watchdog::Feed();

    Serial.println("INFO: Running mainloop");
    DEBUG_PRINTF("Free heap: %u\n",esp_get_free_heap_size());
    DEBUG_PRINTF("Number device discovered: %d\n", BLETrackedDevices.size());

    if (BLETrackedDevices.size() == SettingsMngr.GetMaxNumOfTraceableDevices())
    {
      DEBUG_PRINTLN("INFO: Restart because the array is full\n");
      LOG_TO_FILE("Restarting: reached the max number of traceable devices");
      esp_restart();
    }

    //Reset the states of discovered devices
    for (auto &trackedDevice : BLETrackedDevices)
    {
      trackedDevice.advertised = false;
      trackedDevice.rssiValue = -100;
      trackedDevice.advertisementCounter = 0;
    }

    //DEBUG_PRINTF("\n*** Memory Before scan: %u\n",xPortGetFreeHeapSize());
    pBLEScan->start(SettingsMngr.scanPeriod);
    pBLEScan->stop();
    pBLEScan->clearResults();
    //DEBUG_PRINTF("\n*** Memory After scan: %u\n",xPortGetFreeHeapSize());

#if USE_MQTT
    publishAvailabilityToMQTT();
#endif

    for (auto &trackedDevice : BLETrackedDevices)
    {
      if (trackedDevice.isDiscovered && (trackedDevice.lastDiscoveryTime + MAX_NON_ADV_PERIOD) < NTPTime::seconds())
      {
        trackedDevice.isDiscovered = false;
        FastDiscovery[trackedDevice.address]= false;
        LOG_TO_FILE_D("Devices %s is gone out of range", trackedDevice.address);
      }
    }

#if PUBLISH_BATTERY_LEVEL
    batteryTask();
#endif

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

    publishAvailabilityToMQTT();
#endif
  }
  catch (std::exception &e)
  {
    DEBUG_PRINTF("Error Caught Exception %s", e.what());
    LOG_TO_FILE_E("Error Caught Exception %s", e.what());
  }
  catch (...)
  {
    DEBUG_PRINTLN("Error Unhandled exception trapped in main loop");
    LOG_TO_FILE_E("Error Unhandled exception trapped in main loop");
  }
}
