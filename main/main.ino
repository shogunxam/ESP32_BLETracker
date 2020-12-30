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
      FILE_LOG_WRITE("Error: MQTT broker disconnected, connecting...");
    DEBUG_PRINTLN(F("Error: MQTT broker disconnected, connecting..."));
    if (mqttClient.connect(GATEWAY_NAME, SettingsMngr.mqttUser.c_str(), SettingsMngr.mqttPwd.c_str(), MQTT_AVAILABILITY_TOPIC, 1, true, MQTT_PAYLOAD_UNAVAILABLE))
    {
      DEBUG_PRINTLN(F("INFO: The client is successfully connected to the MQTT broker"));
      FILE_LOG_WRITE("MQTT broker connected!");
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
        FILE_LOG_WRITE("Error: Connection to the MQTT broker failed!");
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
}

/*
  Function called to publish to a MQTT topic with the given payload
*/
void _publishToMQTT(const String &topic, const String &payload, bool retain)
{
  if (mqttClient.publish(topic.c_str(), payload.c_str(), retain))
  {
    DEBUG_PRINTF("INFO: MQTT message published successfully, topic: %s , payload: %s , retain: %s \n", topic.c_str(), payload.c_str(), retain ? "True" : "False");
  }
  else
  {
    DEBUG_PRINTF("ERROR: MQTT message not published, either connection lost, or message too large. Topic: %s , payload: %s , retain: %s \n", topic.c_str(), payload.c_str(), retain ? "True" : "False");
    FILE_LOG_WRITE("Error: MQTT message not published");
  }
}

void publishToMQTT(const String &topic, const String &payload, bool retain)
{
  connectToMQTT();
  _publishToMQTT(topic, payload, retain);
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
  void onResult(BLEAdvertisedDevice advertisedDevice) override
  {
    Watchdog::Feed();
    bool foundPreviouslyAdvertisedDevice = false;
    std::string std_address = advertisedDevice.getAddress().toString();
    String address(std_address.c_str());
    address.replace(":", "");
    address.toUpperCase();

    if (!SettingsMngr.IsTraceable(address))
      return;

    char shortName[31];
    memset(shortName, 0, 31);
    if (advertisedDevice.haveName())
      strncpy(shortName, advertisedDevice.getName().c_str(), 30);

    int RSSI = advertisedDevice.getRSSI();
    for (auto &trackedDevice : BLETrackedDevices)
    {
      foundPreviouslyAdvertisedDevice = address == trackedDevice.address;
      if (foundPreviouslyAdvertisedDevice)
      {
        if (!trackedDevice.advertised)
        {
          trackedDevice.advertised = true;
          if (!trackedDevice.isDiscovered)
          {
            trackedDevice.isDiscovered = true;
            trackedDevice.lastDiscovery = millis();
            trackedDevice.connectionRetry = 0;
            trackedDevice.rssi = String(RSSI);
            DEBUG_PRINTF("INFO: Tracked device newly discovered, Address: %s , RSSI: %d\n", address.c_str(), RSSI);
            if (advertisedDevice.haveName())
            {
              FILE_LOG_WRITE("Device %s ( %s ) within range, RSSI: %d ", address.c_str(), shortName, RSSI);
            }
            else
              FILE_LOG_WRITE("Device %s within range, RSSI: %d ", address.c_str(), RSSI);
          }
          else
          {
            trackedDevice.lastDiscovery = millis();
            trackedDevice.rssi = String(RSSI);
            DEBUG_PRINTF("INFO: Tracked device discovered, Address: %s , RSSI: %d\n", address.c_str(), RSSI);
          }
        }
        break;
      }
    }

    //This is a new device...
    if (!foundPreviouslyAdvertisedDevice)
    {
      BLETrackedDevice trackedDevice;
      trackedDevice.advertised = true;
      trackedDevice.address = address;
      trackedDevice.addressType = advertisedDevice.getAddressType();
      trackedDevice.isDiscovered = true;
      trackedDevice.lastDiscovery = millis();
      trackedDevice.lastBattMeasure = 0;
      trackedDevice.batteryLevel = -1;
      trackedDevice.hasBatteryService = true;
      trackedDevice.connectionRetry = 0;
      trackedDevice.rssi = String(RSSI);
      BLETrackedDevices.push_back(std::move(trackedDevice));

      DEBUG_PRINTF("INFO: Device discovered, Address: %s , RSSI: %d\n", address.c_str(), RSSI);
      if (advertisedDevice.haveName())
        FILE_LOG_WRITE("Discovered new device %s ( %s ) within range, RSSI: %d ", address.c_str(), shortName, RSSI);
      else
        FILE_LOG_WRITE("Discovered new device %s within range, RSSI: %d ", address.c_str(), RSSI);
    }
  }
};

static BLEUUID service_BATT_UUID(BLEUUID((uint16_t)0x180F));
static BLEUUID char_BATT_UUID(BLEUUID((uint16_t)0x2A19));

#if PUBLISH_BATTERY_LEVEL
static EventGroupHandle_t connection_event_group;
const int CONNECTED_EVENT = BIT0;
const int DISCONNECTED_EVENT = BIT1;

class MyBLEClientCallBack : public BLEClientCallbacks
{
  void onConnect(BLEClient *pClient)
  {
  }

  virtual void onDisconnect(BLEClient *pClient)
  {
    log_i(" >> onDisconnect callback");
    xEventGroupSetBits(connection_event_group, DISCONNECTED_EVENT);
    log_i(" << onDisconnect callback");
  }
};

void batteryTask()
{
  //DEBUG_PRINTF("\n*** Memory before battery scan: %u\n",xPortGetFreeHeapSize());

  for (auto &trackedDevice : BLETrackedDevices)
  {
    Watchdog::Feed();
    publishAvailabilityToMQTT();

    if (!SettingsMngr.InBatteryList(trackedDevice.address))
      continue;

    //We need to connect to the device to read the battery value
    //So that we check only the device really advertised by the scan
    unsigned long BatteryReadTimeout = trackedDevice.lastBattMeasure + BATTERY_READ_PERIOD;
    unsigned long BatteryRetryTimeout = trackedDevice.lastBattMeasure + BATTERY_RETRY_PERIOD;
    unsigned long now = millis();
    if (trackedDevice.advertised && trackedDevice.hasBatteryService &&
        ((trackedDevice.batteryLevel > 0 && (BatteryReadTimeout < now)) ||
         (trackedDevice.batteryLevel <= 0 && (BatteryRetryTimeout < now)) ||
         (trackedDevice.lastBattMeasure == 0)))
    {
      DEBUG_PRINTF("\nReading Battery level for %s: Retries: %d\n", trackedDevice.address.c_str(), trackedDevice.connectionRetry);
      bool connectionExtabilished = batteryLevel(trackedDevice.address, trackedDevice.addressType, trackedDevice.batteryLevel, trackedDevice.hasBatteryService);
      if (connectionExtabilished || !trackedDevice.hasBatteryService)
      {
        log_i("Device %s has battery service: %s", trackedDevice.address.c_str(), trackedDevice.hasBatteryService ? "YES" : "NO");
        trackedDevice.connectionRetry = 0;
        trackedDevice.lastBattMeasure = now;
      }
      else
      {
        trackedDevice.connectionRetry++;
        if (trackedDevice.connectionRetry >= MAX_BLE_CONNECTION_RETRIES)
        {
          trackedDevice.connectionRetry = 0;
          trackedDevice.lastBattMeasure = now;
        }
      }
    }
    else if (BatteryReadTimeout < now)
    {
      //Here we preserve the lastBattMeasure time to trigger a new read
      //when the device will be advertised again
      trackedDevice.batteryLevel = -1;
    }
  }
  //DEBUG_PRINTF("\n*** Memory after battery scan: %u\n",xPortGetFreeHeapSize());
}

bool batteryLevel(const String &address, esp_ble_addr_type_t addressType, int &battLevel, bool &hasBatteryService)
{
  log_i(">> ------------------batteryLevel----------------- ");
  bool bleconnected;
  static BLEClient *pClient = nullptr;
  battLevel = -1;
  std::ostringstream osAddress;
  int len = address.length();
  for (int i = 0; i < len; i++)
  {
    osAddress << address[i];
    if ((i & 1) == 1 && i != len - 1)
      osAddress << ':';
  }

  BLEAddress bleAddress = BLEAddress(osAddress.str());
  log_i("connecting to : %s", bleAddress.toString().c_str());
  FILE_LOG_WRITE("Reading battery level for device %s", address.c_str());

  // create a new client
  if(pClient==nullptr)
  {
    pClient = BLEDevice::createClient();
    if(pClient == nullptr)
    {
      DEBUG_PRINTLN("Failed to create BLEClient");
      FILE_LOG_WRITE("Error: Failed to create BLEClient");
      return false;
    }
    else
      log_i("Created client");
    pClient->setClientCallbacks(new MyBLEClientCallBack());
  }

  // Connect to the remote BLE Server.
  bool result = false;

  bleconnected = pClient->connect(bleAddress, addressType);
  if (bleconnected)
  {
    log_i("Connected to server");
    BLERemoteService *pRemote_BATT_Service = pClient->getService(service_BATT_UUID);
    if (pRemote_BATT_Service == nullptr)
    {
      log_i("Cannot find the BATTERY service.");
      FILE_LOG_WRITE("Cannot find the BATTERY service for the device %s", address.c_str());
      hasBatteryService = false;
    }
    else
    {
      BLERemoteCharacteristic *pRemote_BATT_Characteristic = pRemote_BATT_Service->getCharacteristic(char_BATT_UUID);
      if (pRemote_BATT_Characteristic == nullptr)
      {
        log_i("Cannot find the BATTERY characteristic.");
        FILE_LOG_WRITE("Cannot find the BATTERY characteristic for device %s", address.c_str());
        hasBatteryService = false;
      }
      else
      {
        result = true;
        std::string value = pRemote_BATT_Characteristic->readValue();
        if (value.length() > 0)
          battLevel = (int)value[0];
        log_i("Reading BATTERY level : %d", battLevel);
        FILE_LOG_WRITE("Battery level for device %s is %d", address.c_str(), battLevel);
        hasBatteryService = true;
      }
    }
    //Before disconnecting I need to pause the task to wait (I'don't know what), otherwhise we have an heap corruption
    delay(100);
    log_i("disconnecting...");
    pClient->disconnect();
    EventBits_t bits = xEventGroupWaitBits(connection_event_group, DISCONNECTED_EVENT, true, true, portMAX_DELAY);
    log_i("wait for disconnection done: %d", bits);
  }
  else
  {
    log_i("-------------------Not connected!!!--------------------");
    FILE_LOG_WRITE("Connection to device %s failed", address.c_str());
  }

  log_i("<< ------------------batteryLevel----------------- ");
  return bleconnected;
}
#endif

void LogResetReason()
{
  esp_reset_reason_t r = esp_reset_reason();
  String msg;
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
  DEBUG_PRINTLN(msg.c_str());
  FILE_LOG_WRITE(msg.c_str());
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

#if ENABLE_FILE_LOG
  SPIFFSLogger.Initialize("/logs.bin", 200);
#endif

  WiFiConnect(WIFI_SSID, WIFI_PASSWORD);

  LogResetReason();

#if ERASE_DATA_AFTER_FLASH
  if (dataErased == 1)
    FILE_LOG_WRITE("Error Erasing all persitent data!");
  else if (dataErased == 2)
    FILE_LOG_WRITE("Erased all persitent data!");
#endif

#if ENABLE_OTA_WEBSERVER
  webserver.setup(GATEWAY_NAME, WIFI_SSID, WIFI_PASSWORD);
  webserver.begin();
#endif

  SettingsMngr.SettingsFile(F("/settings.bin"));
  if (SPIFFS.exists(F("/settings.bin")))
    SettingsMngr.Load();

  BLETrackedDevices.reserve(SettingsMngr.GetMaxNumOfTraceableDevices());

  Watchdog::Initialize();

  BLEDevice::init(GATEWAY_NAME);
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(false);

  mqttClient.setServer(SettingsMngr.mqttServer.c_str(), SettingsMngr.mqttPort);
  connectToMQTT();

#if PUBLISH_BATTERY_LEVEL
  connection_event_group = xEventGroupCreate();
#endif

  FILE_LOG_WRITE("BLETracker initialized");
}

void publishBLEState(const String &address, const String &state, const String &rssi, int batteryLevel)
{
  String baseTopic = MQTT_BASE_SENSOR_TOPIC;
  baseTopic += "/" + address;

#if PUBLISH_SEPARATED_TOPICS
  String stateTopic = baseTopic + "/state";
  String rssiTopic = baseTopic + "/rssi";
  String batteryTopic = baseTopic + "/battery";
  publishToMQTT(stateTopic, state, false);
  publishToMQTT(rssiTopic, rssi, false);
#if PUBLISH_BATTERY_LEVEL
  publishToMQTT(batteryTopic, String(batteryLevel), false);
#endif
#endif

#if PUBLISH_SIMPLE_JSON
  String payload = R"({ "state":")" + state + R"(","rssi":)" + rssi;
#if PUBLISH_BATTERY_LEVEL
  payload += R"(,"battery":)" + String(batteryLevel);
#endif
  payload += "}";

  publishToMQTT(baseTopic, payload, false);
#endif
}

String formatMillis(unsigned long milliseconds)
{
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;
  std::ostringstream ostime;
  ostime << days << "." << std::setfill('0') << std::setw(2) << (hours % 24) << ":"
         << std::setfill('0') << std::setw(2) << (minutes % 60) << ":"
         << std::setfill('0') << std::setw(2) << (seconds % 60);
  return ostime.str().c_str();
}

void publishSySInfo()
{
  String sysTopic = MQTT_BASE_SENSOR_TOPIC "/sysinfo";
  String IP = WiFi.localIP().toString();

  String payload = R"({ "uptime":")" + formatMillis(millis()) + R"(","version":)" VERSION R"(,"SSID":")" WIFI_SSID R"(", "IP":")" + IP + R"("})";
  publishToMQTT(sysTopic, payload, false);
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
      FILE_LOG_WRITE("Restart reached max number of traceable devices");
      esp_restart();
    }

    for (auto &trackedDevice : BLETrackedDevices)
    {
      trackedDevice.advertised = false;
      trackedDevice.rssi = String(F("-100"));
    }

    //DEBUG_PRINTF("\n*** Memory Before scan: %u\n",xPortGetFreeHeapSize());
    pBLEScan->start(SettingsMngr.scanPeriod);
    pBLEScan->stop();
    pBLEScan->clearResults();
    //DEBUG_PRINTF("\n*** Memory After scan: %u\n",xPortGetFreeHeapSize());

    publishAvailabilityToMQTT();

    for (auto &trackedDevice : BLETrackedDevices)
    {
      if (trackedDevice.isDiscovered == true && (trackedDevice.lastDiscovery + MAX_NON_ADV_PERIOD) < millis())
      {
        trackedDevice.isDiscovered = false;
        FILE_LOG_WRITE("Devices %s is gone out of range", trackedDevice.address.c_str());
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
        publishBLEState(trackedDevice.address, MQTT_PAYLOAD_ON, trackedDevice.rssi, trackedDevice.batteryLevel);
      }
      else
      {
        publishBLEState(trackedDevice.address, MQTT_PAYLOAD_OFF, "-100", trackedDevice.batteryLevel);
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
    FILE_LOG_WRITE("Error Caught Exception %s", e.what());
  }
  catch (...)
  {
    DEBUG_PRINTLN("Error Unhandled exception trapped in main loop");
    FILE_LOG_WRITE("Error Unhandled exception trapped in main loop");
  }
}
