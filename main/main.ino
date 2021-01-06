#include "main.h"

#include <BLEDevice.h>
#include <sstream>
#include <iomanip>
#include "WiFiManager.h"

#include "config.h"

#define CONFIG_ESP32_DEBUG_OCDAWARE 1

// MQTT_KEEPALIVE : keepAlive interval in Seconds
// Keepalive timeout for default MQTT Broker is 10s
#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE 45
#endif

// MQTT_SOCKET_TIMEOUT: socket timeout interval in Seconds
#ifndef MQTT_SOCKET_TIMEOUT
#define MQTT_SOCKET_TIMEOUT 60
#endif

#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient

#include "esp_system.h"
#include "DebugPrint.h"

#if ENABLE_OTA_WEBSERVER
#include "OTAWebServer.h"
#endif

#include "settings.h"
#include "watchdog.h"

#include "SPIFFSLogger.h"

char _printbuffer_[256];
std::mutex _printLock_;

std::vector<BLETrackedDevice> BLETrackedDevices;

BLEScan *pBLEScan;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

#define SYS_INFORMATION_DELAY 120000 /*2 minutes*/
unsigned long lastSySInfoTime = 0;

#if ENABLE_OTA_WEBSERVER
OTAWebServer webserver;
#endif

///////////////////////////////////////////////////////////////////////////
//   MQTT
///////////////////////////////////////////////////////////////////////////
volatile unsigned long lastMQTTConnection = 0;
/*
  Function called to connect/reconnect to the MQTT broker
*/
static bool firstTimeMQTTConnection = true;
static bool MQTTConnectionErrorSignaled = false;
static uint32_t MQTTErrorCounter = 0;
bool connectToMQTT()
{
  WiFiConnect(WIFI_SSID, WIFI_PASSWORD);

  uint8_t maxRetry = 3;
  while (!mqttClient.connected())
  {
    DEBUG_PRINTF("INFO: Connecting to MQTT broker: %s\n", SettingsMngr.mqttServer.c_str());
    if (!firstTimeMQTTConnection && !MQTTConnectionErrorSignaled)
      LOG_TO_FILE_E("Error: MQTT broker disconnected, connecting...");
    DEBUG_PRINTLN(F("Error: MQTT broker disconnected, connecting..."));
    if (mqttClient.connect(GATEWAY_NAME, SettingsMngr.mqttUser.c_str(), SettingsMngr.mqttPwd.c_str(), MQTT_AVAILABILITY_TOPIC, 1, true, MQTT_PAYLOAD_UNAVAILABLE))
    {
      DEBUG_PRINTLN(F("INFO: The client is successfully connected to the MQTT broker"));
      LOG_TO_FILE_I("MQTT broker connected!");
      MQTTConnectionErrorSignaled = false;
      _publishToMQTT(MQTT_AVAILABILITY_TOPIC, MQTT_PAYLOAD_AVAILABLE, true);
    }
    else
    {
      DEBUG_PRINTLN(F("ERROR: The connection to the MQTT broker failed"));
      DEBUG_PRINTF("INFO: MQTT username: %s\n", SettingsMngr.mqttUser.c_str())
      DEBUG_PRINTF("INFO: MQTT password: %s\n", SettingsMngr.mqttPwd.c_str());
      DEBUG_PRINTF("INFO: MQTT broker: %s\n", SettingsMngr.mqttServer.c_str());
#if ENABLE_FILE_LOG
      uint32_t numLogs = SPIFFSLogger.numOfLogsPerSession();
      uint32_t deltaLogs;

      if (numLogs < MQTTErrorCounter)
        deltaLogs = ~uint32_t(0) - MQTTErrorCounter + numLogs;
      else
        deltaLogs = numLogs - MQTTErrorCounter;

      if (deltaLogs >= 500)
        MQTTConnectionErrorSignaled = false;

      if (!MQTTConnectionErrorSignaled)
      {
        LOG_TO_FILE_E("Error: Connection to the MQTT broker failed!");
        MQTTConnectionErrorSignaled = true;
        MQTTErrorCounter = SPIFFSLogger.numOfLogsPerSession();
      }
#endif
    }

    Watchdog::Feed();

    maxRetry--;
    if (maxRetry == 0)
      return false;
  }
  firstTimeMQTTConnection = false;
  return true;
}

/*
  Function called to publish to a MQTT topic with the given payload
*/
void _publishToMQTT(const char *topic, const char *payload, bool retain)
{
  if (mqttClient.publish(topic, payload, retain))
  {
    DEBUG_PRINTF("INFO: MQTT message published successfully, topic: %s , payload: %s , retain: %s \n", topic, payload, retain ? "True" : "False");
  }
  else
  {
    DEBUG_PRINTF("ERROR: MQTT message not published, either connection lost, or message too large. Topic: %s , payload: %s , retain: %s \n", topic, payload, retain ? "True" : "False");
    LOG_TO_FILE_E("Error: MQTT message not published");
  }
}

void publishToMQTT(const char *topic, const char *payload, bool retain)
{
  if (connectToMQTT())
  {
    _publishToMQTT(topic, payload, retain);
    delay(100);
  }
}

void publishAvailabilityToMQTT()
{
  if (millis() > lastMQTTConnection)
  {
    lastMQTTConnection = millis() + MQTT_CONNECTION_TIME_OUT;
    DEBUG_PRINTF("INFO: MQTT availability topic: %s\n", MQTT_AVAILABILITY_TOPIC);
    publishToMQTT(MQTT_AVAILABILITY_TOPIC, MQTT_PAYLOAD_AVAILABLE, true);
  }
}

///////////////////////////////////////////////////////////////////////////
//   BLUETOOTH
///////////////////////////////////////////////////////////////////////////
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void NormalizeAddress(const std::string &in, char out[ADDRESS_STRING_SIZE])
  {
    char *outWlkr = out;
    for (int i = 0; i < in.length(); i++)
    {
      const char c = in.at(i);
      if (c == ':')
        continue;
      *outWlkr = toupper(c);
      outWlkr++;
    }
    out[ADDRESS_STRING_SIZE - 1] = '\0';
  }

  void onResult(BLEAdvertisedDevice advertisedDevice) override
  {
    Watchdog::Feed();
    const uint8_t shortNameSize = 31;
    std::string std_address = advertisedDevice.getAddress().toString();
    char address[ADDRESS_STRING_SIZE];
    NormalizeAddress(std_address, address);

    if (!SettingsMngr.IsTraceable(address))
      return;

    char shortName[shortNameSize];
    memset(shortName, 0, shortNameSize);
    if (advertisedDevice.haveName())
      strncpy(shortName, advertisedDevice.getName().c_str(), shortNameSize - 1);

    int RSSI = advertisedDevice.getRSSI();
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

        if (!trackedDevice.advertised)//Skip advertised dups
        {
          trackedDevice.addressType = advertisedDevice.getAddressType();
          trackedDevice.advertised = true;
          if (!trackedDevice.isDiscovered)
          {
            trackedDevice.isDiscovered = true;
            trackedDevice.lastDiscoveryTime = millis();
            trackedDevice.connectionRetry = 0;
            trackedDevice.rssiValue = RSSI;
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
            trackedDevice.lastDiscoveryTime = millis();
            trackedDevice.rssiValue = RSSI;
            DEBUG_PRINTF("INFO: Tracked device discovered, Address: %s , RSSI: %d\n", address, RSSI);
          }
        }
        return;
      }
    }

    //This is a new device...
    BLETrackedDevice trackedDevice;
    trackedDevice.advertised = true; //Skip duplicates
    memcpy(trackedDevice.address, address, ADDRESS_STRING_SIZE);
    trackedDevice.addressType = advertisedDevice.getAddressType();
    trackedDevice.isDiscovered = true;
    trackedDevice.lastDiscoveryTime = millis();
    trackedDevice.lastBattMeasureTime = 0;
    trackedDevice.batteryLevel = -1;
    trackedDevice.hasBatteryService = true;
    trackedDevice.connectionRetry = 0;
    trackedDevice.rssiValue = RSSI;
    BLETrackedDevices.push_back(std::move(trackedDevice));

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

    publishAvailabilityToMQTT();

    //We need to connect to the device to read the battery value
    //So that we check only the device really advertised by the scan
    unsigned long BatteryReadTimeout = trackedDevice.lastBattMeasureTime + BATTERY_READ_PERIOD;
    unsigned long BatteryRetryTimeout = trackedDevice.lastBattMeasureTime + BATTERY_RETRY_PERIOD;
    unsigned long now = millis();
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

void DenormalizeAddress(const char address[ADDRESS_STRING_SIZE], char out[ADDRESS_STRING_SIZE + 5])
{
  char *outWlkr = out;
  for (int i = 0; i < ADDRESS_STRING_SIZE; i++)
  {
    *outWlkr = address[i];
    outWlkr++;
    if ((i & 1) == 1 && i != (ADDRESS_STRING_SIZE - 2))
    {
      *outWlkr = ':';
      outWlkr++;
    }
  }
}

bool batteryLevel(const char address[ADDRESS_STRING_SIZE], esp_ble_addr_type_t addressType, int8_t &battLevel, bool &hasBatteryService)
{
  log_i(">> ------------------batteryLevel----------------- ");
  bool bleconnected;
  BLEClient *pClient = nullptr;
  battLevel = -1;
  static char denomAddress[ADDRESS_STRING_SIZE + 5];
  DenormalizeAddress(address, denomAddress);
  BLEAddress bleAddress = BLEAddress(denomAddress);
  log_i("connecting to : %s", bleAddress.toString().c_str());
  LOG_TO_FILE_D("Reading battery level for device %s", address);
  MyBLEClientCallBack callback;
  // create a new client each client is unique
  if (pClient == nullptr)
  {
    pClient = new BLEClient();
    if (pClient == nullptr)
    {
      DEBUG_PRINTLN("Failed to create BLEClient");
      LOG_TO_FILE_E("Error: Failed to create BLEClient");
      return false;
    }
    else
      log_i("Created client");
    pClient->setClientCallbacks(&callback);
  }

  // Connect to the remote BLE Server.
  bleconnected = pClient->connect(bleAddress, addressType);
  if (bleconnected)
  {
    log_i("Connected to server");
    BLERemoteService *pRemote_BATT_Service = pClient->getService(service_BATT_UUID);
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
    if(pClient->isConnected())
    {
      log_i("disconnecting...");
      pClient->disconnect();
    }  
    log_i("waiting for disconnection...");
    while(pClient->isConnected())
      delay(100);
    log_i("Client disconnected.");
    //EventBits_t bits = xEventGroupWaitBits(connection_event_group, DISCONNECTED_EVENT, true, true, portMAX_DELAY);
    //log_i("wait for disconnection done: %d", bits);
  }
  else
  {
    //We fail to connect and we have to be sure the PeerDevice is removed before delete it
    BLEDevice::removePeerDevice(pClient->m_appId,true);
    log_i("-------------------Not connected!!!--------------------");
    LOG_TO_FILE_E("Error: Connection to device %s failed", address);
  }

  if (pClient != nullptr)
  {
    log_i("delete Client.");
    delete pClient;
    pClient = nullptr;
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

  const char* settingsFile = "/settings.bin";
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
  pBLEScan->setActiveScan(false);
  pBLEScan->setInterval(50);
  pBLEScan->setWindow(50);

  mqttClient.setServer(SettingsMngr.mqttServer.c_str(), SettingsMngr.mqttPort);
  connectToMQTT();

#if PUBLISH_BATTERY_LEVEL
  //connection_event_group = xEventGroupCreate();
#endif

  LOG_TO_FILE("BLETracker initialized");
}

void publishBLEState(const char address[ADDRESS_STRING_SIZE], const char state[4], int8_t rssi, int8_t batteryLevel)
{
  constexpr uint16_t maxTopicLen = sizeof(MQTT_BASE_SENSOR_TOPIC) + 22;
  char topic[maxTopicLen];
  char strbuff[5];

#if PUBLISH_SEPARATED_TOPICS
  snprintf(topic, maxTopicLen, "%s/%s/state", MQTT_BASE_SENSOR_TOPIC, address);
  publishToMQTT(topic, state, false);
  snprintf(topic, maxTopicLen, "%s/%s/rssi", MQTT_BASE_SENSOR_TOPIC, address);
  itoa(rssi, strbuff, 10);
  publishToMQTT(topic, strbuff, false);
#if PUBLISH_BATTERY_LEVEL
  snprintf(topic, maxTopicLen, "%s/%s/battery", MQTT_BASE_SENSOR_TOPIC, address);
  itoa(batteryLevel, strbuff, 10);
  publishToMQTT(topic, strbuff, false);
#endif
#endif

#if PUBLISH_SIMPLE_JSON
  snprintf(topic, maxTopicLen, "%s/%s", MQTT_BASE_SENSOR_TOPIC, address);
  const uint16_t maxPayloadLen = 45;
  char payload[maxPayloadLen];
  snprintf(payload, maxPayloadLen, R"({"state":"%s","rssi":%d,"battery":%d})", state, rssi, batteryLevel);
#if PUBLISH_BATTERY_LEVEL
  snprintf(payload, maxPayloadLen, R"({"state":"%s","rssi":%d,"battery":%d})", state, rssi, batteryLevel);
#else
  snprintf(payload, maxPayloadLen, R"({"state":"%s","rssi":%d})", state, rssi);
#endif
  publishToMQTT(topic, payload, false);
#endif
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

#define SYS_TOPIC MQTT_BASE_SENSOR_TOPIC "/sysinfo"
void publishSySInfo()
{
  constexpr uint16_t maxSysPayloadLen = 83 + sizeof(VERSION) + sizeof(WIFI_SSID);
  char sysPayload[maxSysPayloadLen];
  static String IP;
  IP.reserve(16);
  IP = WiFi.localIP().toString();
  char strmilli[20];
  snprintf(sysPayload, maxSysPayloadLen, R"({"uptime":"%s","version":"%s","SSID":"%s","IP":"%s"})", formatMillis(millis(), strmilli), VERSION, WIFI_SSID, IP.c_str());
  publishToMQTT(SYS_TOPIC, sysPayload, false);
}

void loop()
{
  try
  {
    mqttClient.loop();
    Watchdog::Feed();

    Serial.println("INFO: Running mainloop");
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

    publishAvailabilityToMQTT();

    for (auto &trackedDevice : BLETrackedDevices)
    {
      if (trackedDevice.isDiscovered && (trackedDevice.lastDiscoveryTime + MAX_NON_ADV_PERIOD) < millis())
      {
        trackedDevice.isDiscovered = false;
        LOG_TO_FILE_D("Devices %s is gone out of range", trackedDevice.address);
      }
    }

#if PUBLISH_BATTERY_LEVEL
    batteryTask();
#endif

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
    if (((lastSySInfoTime + SYS_INFORMATION_DELAY) < millis()) || (lastSySInfoTime == 0))
    {
      publishSySInfo();
      lastSySInfoTime = millis();
    }

    publishAvailabilityToMQTT();
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
