/*
  MQTT Binary Sensor - Bluetooth LE Device Tracker - Home Assistant

  Libraries:
    - PubSubClient: https://github.com/knolleary/pubsubclient
    - ESP32 BLE:    https://github.com/nkolban/ESP32_BLE_Arduino

  Sources:
    - https://github.com/nkolban/ESP32_BLE_Arduino/blob/master/examples/BLE_scan/BLE_scan.ino
    - https://www.youtube.com/watch?v=KNoFdKgvskU

  Samuel M. - v1.0 - 01.2018
  If you like this example, please add a star! Thank you!
  https://github.com/mertenats/open-home-automation

  SDeSalve -  v4.2 - 21.08.2019
  https://github.com/sdesalve/dss_mqtt_binary_sensor_ble_scanner
*/
#define CONFIG_ESP32_DEBUG_OCDAWARE 1
typedef struct
{
  String address;
  char rssi[4];
  bool isDiscovered;
  long lastDiscovery;
  long lastBattMeasure;
  int batteryLevel;
  bool advertised;
} BLETrackedDevice;

#include "config.h"

#include <BLEDevice.h>
#include <WiFi.h>
#include <sstream>

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

static char _printbuffer_[256];
#if defined(DEBUG_SERIAL)
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(x,...) {\
snprintf(_printbuffer_,255,x,__VA_ARGS__);\
Serial.print(_printbuffer_);\
}
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(x,...) 
#endif
#define SERIAL_PRINTF(x,...) {\
snprintf(_printbuffer_,255,x,__VA_ARGS__);\
Serial.print(_printbuffer_);\
}

BLEScan *pBLEScan;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

#define VERSION 1.0
#define SYS_INFORMATION_DELAY 120000 /*2 minutes*/
unsigned long lastSySInfoTime = 0;

///////////////////////////////////////////////////////////////////////////
//   MQTT
///////////////////////////////////////////////////////////////////////////
volatile unsigned long lastMQTTConnection = 0;
/*
  Function called to publish to a MQTT topic with the given payload
*/
void publishToMQTT(const char *p_topic, const char *p_payload, bool p_retain)
{
  while (!mqttClient.connected())
  {
    DEBUG_PRINTF("INFO: Connecting to MQTT broker: %s\n",MQTT_SERVER);
    connectToMQTT();
    delay(500);
  }

  if (mqttClient.publish(p_topic, p_payload, p_retain))
  {
    DEBUG_PRINTF("INFO: MQTT message published successfully, topic: %s , payload: %s , retain: %s \n",p_topic,p_payload, p_retain ? "True":"False");
  }
  else
  {
    DEBUG_PRINTF("ERROR: MQTT message not published, either connection lost, or message too large. Topic: %s , payload: %s , retain: %s \n",p_topic,p_payload, p_retain ? "True":"False");
  }
}
/*
  Function called to connect/reconnect to the MQTT broker
*/
void connectToMQTT()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    DEBUG_PRINTF("INFO: WiFi connecting to: %s\n",WIFI_SSID);
    delay(10);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    randomSeed(micros());

    while (WiFi.status() != WL_CONNECTED)
    {
      DEBUG_PRINTF(".",nullptr);
      delay(500);
    }

    DEBUG_PRINTF("Connected to WiFi. Current IP: %s\n",WiFi.localIP().toString().c_str());
  }

  DEBUG_PRINTF("INFO: MQTT availability topic: %s\n",MQTT_AVAILABILITY_TOPIC);
  //DEBUG_PRINT(F("INFO: MQTT availability topic: "));
  //DEBUG_PRINTLN(MQTT_AVAILABILITY_TOPIC);

  if (!mqttClient.connected())
  {
    if (mqttClient.connect(GATEWAY_NAME, MQTT_USERNAME, MQTT_PASSWORD, MQTT_AVAILABILITY_TOPIC, 1, true, MQTT_PAYLOAD_UNAVAILABLE))
    {
      DEBUG_PRINTLN(F("INFO: The client is successfully connected to the MQTT broker"));
      publishToMQTT(MQTT_AVAILABILITY_TOPIC, MQTT_PAYLOAD_AVAILABLE, true);
    }
    else
    {
      DEBUG_PRINTLN(F("ERROR: The connection to the MQTT broker failed"));
      DEBUG_PRINTF("INFO: MQTT username: %s\n",MQTT_USERNAME)
      DEBUG_PRINTF("INFO: MQTT password: %s\n",MQTT_PASSWORD);
      DEBUG_PRINTF("INFO: MQTT broker: %s\n",MQTT_SERVER);
    }
  }
  else
  {
    if (lastMQTTConnection < millis())
    {
      lastMQTTConnection = millis() + MQTT_CONNECTION_TIMEOUT;
      publishToMQTT(MQTT_AVAILABILITY_TOPIC, MQTT_PAYLOAD_AVAILABLE, true);
    }
  }
}

hw_timer_t *timer = NULL;
void IRAM_ATTR resetModule()
{
  ets_printf("INFO: Reboot\n");
  esp_restart();
}

///////////////////////////////////////////////////////////////////////////
//   BLUETOOTH
///////////////////////////////////////////////////////////////////////////
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    bool findedAdvertisedDevice = false;
    String address(advertisedDevice.getAddress().toString().c_str());
    address.replace(":", "");
    address.toUpperCase();
    for (uint8_t i = 0; i < NB_OF_BLE_DISCOVERED_DEVICES; i++)
    {
      if (address == BLETrackedDevices[i].address)
      {
        BLETrackedDevices[i].advertised = true;
        if (!BLETrackedDevices[i].isDiscovered)
        {
          BLETrackedDevices[i].isDiscovered = true;
          BLETrackedDevices[i].lastDiscovery = millis();
          itoa(advertisedDevice.getRSSI(), BLETrackedDevices[i].rssi, 10);
          DEBUG_PRINTF("INFO: Tracked device newly discovered, Address: %s , RSSI: %d",address.c_str(), advertisedDevice.getRSSI());
          //DEBUG_PRINT(F("INFO: Tracked device newly discovered, Address: "));
          //DEBUG_PRINT(address);
          //DEBUG_PRINT(F(", RSSI: "));
          //DEBUG_PRINTLN(advertisedDevice.getRSSI());
        }
        else
        {
          BLETrackedDevices[i].lastDiscovery = millis();
          itoa(advertisedDevice.getRSSI(), BLETrackedDevices[i].rssi, 10);
          DEBUG_PRINTF("INFO: Tracked device discovered, Address: %s , RSSI: %d",address.c_str(), advertisedDevice.getRSSI());
          //DEBUG_PRINT(F("INFO: Tracked device discovered, Address: "));
          //DEBUG_PRINT(address);
          //DEBUG_PRINT(F(", RSSI: "));
          //DEBUG_PRINTLN(advertisedDevice.getRSSI());
        }
        findedAdvertisedDevice = true;
        break;
      }
    }

    if (!findedAdvertisedDevice)
    {
      NB_OF_BLE_DISCOVERED_DEVICES = NB_OF_BLE_DISCOVERED_DEVICES + 1;
      BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].advertised = true;
      BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].address = address;
      BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].isDiscovered = true;
      BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].lastDiscovery = millis();
      BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].lastBattMeasure = 0;
      itoa(advertisedDevice.getRSSI(), BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].rssi, 10);

      DEBUG_PRINTF("INFO: Device discovered, Address: %s , RSSI: %d",address.c_str(), advertisedDevice.getRSSI());
      //DEBUG_PRINT(F("INFO: Device discovered, Address: "));
      //DEBUG_PRINT(advertisedDevice.getAddress().toString().c_str());
      //DEBUG_PRINT(F(", RSSI: "));
    }
  }
};

static BLEUUID service_BATT_UUID(BLEUUID((uint16_t)0x180F));
static BLEUUID char_BATT_UUID(BLEUUID((uint16_t)0x2A19));

#if 1
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

bool batteryLevel(const String &address, int &battLevel)
{
  log_i(">> ------------------batteryLevel----------------- ");
  BLEClient *pClient = nullptr;
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

  // create a new client
  pClient = BLEDevice::createClient();
  log_i("Created client");
  pClient->setClientCallbacks(new MyBLEClientCallBack());

  // Connect to the remote BLE Server.
  bool result = false;
  bool bleconnected = pClient->connect(bleAddress, BLE_ADDR_TYPE_RANDOM);
  if (bleconnected)
  {
    log_i("Connected to server");
    BLERemoteService *pRemote_BATT_Service = pClient->getService(service_BATT_UUID);
    if (pRemote_BATT_Service == nullptr)
    {
      log_i("Failed to find BATTERY service.");
    }
    else
    {
      BLERemoteCharacteristic *pRemote_BATT_Characteristic = pRemote_BATT_Service->getCharacteristic(char_BATT_UUID);
      if (pRemote_BATT_Characteristic == nullptr)
      {
        log_i("Failed to find BATTERY characteristic.");
      }
      else
      {
        result = true;
        std::string value = pRemote_BATT_Characteristic->readValue();
        if (value.length() > 0)
          battLevel = (int)value[0];
        log_i("Reading BATTERY level : %d", battLevel);
      }
    }
    //Before disconnecting I need to pause the task to wait (I'don't know what), otherwhise we have an heap corruption
    vTaskDelay(100 * portTICK_PERIOD_MS);
    log_i("disconnecting...");
    pClient->disconnect();
    EventBits_t bits = xEventGroupWaitBits(connection_event_group, DISCONNECTED_EVENT, true, true, portMAX_DELAY);
    log_i("wait for disconnection done: %d", bits);
  }
  else
  {
    log_i("-------------------Not connected!!!--------------------");
  }
  delete pClient;
  pClient = nullptr;
  log_i("<< ------------------batteryLevel----------------- ");
  return result;
}
#endif
///////////////////////////////////////////////////////////////////////////
//   SETUP() & LOOP()
///////////////////////////////////////////////////////////////////////////
void setup()
{
#if defined(DEBUG_SERIAL)
  Serial.begin(115200);
#endif

  Serial.println("INFO: Running setup");

  timer = timerBegin(0, 80, true); //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);
  timerAlarmWrite(timer, 120000000, false); //set time in us 120000000 = 120 sec
  timerAlarmEnable(timer);                  //enable interrupt

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(false);

  mqttClient.setServer(MQTT_SERVER, MQTT_SERVER_PORT);

  connection_event_group = xEventGroupCreate();
}

void publishBLEState(String address, const char *state, const char *rssi, int batteryLevel)
{
  String baseTopic = MQTT_BASE_SENSOR_TOPIC;
  baseTopic += "/" + address;
  String stateTopic = baseTopic + "/state";
  String rssiTopic = baseTopic + "/rssi";
  String batteryTopic = baseTopic + "/battery";
  publishToMQTT(stateTopic.c_str(), state, false);
  publishToMQTT(rssiTopic.c_str(), rssi, false);
  char batteryStr [5];
  itoa(batteryLevel,batteryStr,10);
  publishToMQTT(batteryTopic.c_str(), batteryStr, false);
  std::ostringstream payload;
  payload << "{ \"state\":\"" << state << "\",\"rssi\":" << rssi <<  ",\"battery\":" << batteryLevel << "}";
  publishToMQTT(baseTopic.c_str(), payload.str().c_str(), false);
}

void publishSySInfo()
{
  String baseTopic = MQTT_BASE_SENSOR_TOPIC;
  String sysTopic = baseTopic + "/sysinfo";
  std::ostringstream payload;
  payload << "{ \"uptime\":\"" << millis() << "\",\"version\":" << VERSION << ",\"SSID\":\"" << WIFI_SSID << "\"}";
  publishToMQTT(sysTopic.c_str(), payload.str().c_str(), false);
}

void loop()
{
  if (lastSySInfoTime == 0)
  {
    lastSySInfoTime = millis();
  }

  mqttClient.loop();
  timerWrite(timer, 0); //reset timer (feed watchdog)
  long tme = millis();
  Serial.println("INFO: Running mainloop");
  DEBUG_PRINTF("Number device discovered: %d\n", NB_OF_BLE_DISCOVERED_DEVICES);
  //DEBUG_PRINTLN(NB_OF_BLE_DISCOVERED_DEVICES);

  if (NB_OF_BLE_DISCOVERED_DEVICES > 90)
  {
    DEBUG_PRINTLN("INFO: Riavvio perché ho finito l'array\n");
    esp_restart();
  }

  for (uint8_t i = 0; i < NB_OF_BLE_DISCOVERED_DEVICES; i++)
    BLETrackedDevices[i].advertised = false;

  pBLEScan->setActiveScan(true);
  pBLEScan->start(BLE_SCANNING_PERIOD);

  for (uint8_t i = 0; i < NB_OF_BLE_DISCOVERED_DEVICES; i++)
  {
    if (BLETrackedDevices[i].isDiscovered == true && BLETrackedDevices[i].lastDiscovery + MAX_NON_ADV_PERIOD < millis())
    {
      BLETrackedDevices[i].isDiscovered = false;
    }
  }

#if 1
  for (uint8_t i = 0; i < NB_OF_BLE_DISCOVERED_DEVICES; i++)
  {
    //We need to connect to the device to read the battery value
    //So that we check only the device really advertised by the scan
    if (BLETrackedDevices[i].advertised && ((BLETrackedDevices[i].lastBattMeasure + MAX_BATTERY_READ_PERIOD) < millis() 
                                            || BLETrackedDevices[i].lastBattMeasure == 0 ))
    {
      DEBUG_PRINTF("\nReading Battery level for %s\n",BLETrackedDevices[i].address.c_str());
      int battLevel;
      batteryLevel(BLETrackedDevices[i].address, BLETrackedDevices[i].batteryLevel);
      BLETrackedDevices[i].lastBattMeasure = millis();
    }
  }
#endif

  for (uint8_t i = 0; i < NB_OF_BLE_DISCOVERED_DEVICES; i++)
  {
    if (BLETrackedDevices[i].isDiscovered)
    {
      publishBLEState(BLETrackedDevices[i].address, MQTT_PAYLOAD_ON, BLETrackedDevices[i].rssi, BLETrackedDevices[i].batteryLevel);
    }
    else
    {
      publishBLEState(BLETrackedDevices[i].address, MQTT_PAYLOAD_OFF, "-100", -1);
    }
  }

  //System Information
  if ((lastSySInfoTime + SYS_INFORMATION_DELAY) < millis())
  {
    publishSySInfo();
    lastSySInfoTime = millis();
  }

  connectToMQTT();
}
