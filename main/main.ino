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

typedef struct
{
  String address;
  char rssi[4];
  bool isDiscovered;
  long lastDiscovery;
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

#if defined(DEBUG_SERIAL)
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

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
    DEBUG_PRINT(F("INFO: Connecting to MQTT broker: "));
    DEBUG_PRINTLN(MQTT_SERVER);
    connectToMQTT();
    delay(500);
  }

  if (mqttClient.publish(p_topic, p_payload, p_retain))
  {
    DEBUG_PRINT(F("INFO: MQTT message published successfully, topic: "));
    DEBUG_PRINT(p_topic);
    DEBUG_PRINT(F(", payload: "));
    DEBUG_PRINT(p_payload);
    DEBUG_PRINT(F(", retain: "));
    DEBUG_PRINTLN(p_retain);
  }
  else
  {
    DEBUG_PRINTLN(F("ERROR: MQTT message not published, either connection lost, or message too large. Topic: "));
    DEBUG_PRINT(p_topic);
    DEBUG_PRINT(F(" , payload: "));
    DEBUG_PRINT(p_payload);
    DEBUG_PRINT(F(", retain: "));
    DEBUG_PRINTLN(p_retain);
  }
}
/*
  Function called to connect/reconnect to the MQTT broker
*/
void connectToMQTT()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    DEBUG_PRINT(F("INFO: WiFi connecting to: "));
    DEBUG_PRINTLN(WIFI_SSID);
    delay(10);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    randomSeed(micros());

    while (WiFi.status() != WL_CONNECTED)
    {
      DEBUG_PRINT(F("."));
      delay(500);
    }
    Serial.println("Connected to WiFi.");
    DEBUG_PRINT("Current IP: ");
    DEBUG_PRINTLN(WiFi.localIP());
  }

  DEBUG_PRINT(F("INFO: MQTT availability topic: "));
  DEBUG_PRINTLN(MQTT_AVAILABILITY_TOPIC);

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
      DEBUG_PRINT(F("INFO: MQTT username: "));
      DEBUG_PRINTLN(MQTT_USERNAME);
      DEBUG_PRINT(F("INFO: MQTT password: "));
      DEBUG_PRINTLN(MQTT_PASSWORD);
      DEBUG_PRINT(F("INFO: MQTT broker: "));
      DEBUG_PRINTLN(MQTT_SERVER);
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
        if (!BLETrackedDevices[i].isDiscovered)
        {
          BLETrackedDevices[i].isDiscovered = true;
          BLETrackedDevices[i].lastDiscovery = millis();
          itoa(advertisedDevice.getRSSI(), BLETrackedDevices[i].rssi, 10);

          DEBUG_PRINT(F("INFO: Tracked device newly discovered, Address: "));
          DEBUG_PRINT(address);
          DEBUG_PRINT(F(", RSSI: "));
          DEBUG_PRINTLN(advertisedDevice.getRSSI());
        }
        else
        {
          BLETrackedDevices[i].lastDiscovery = millis();
          itoa(advertisedDevice.getRSSI(), BLETrackedDevices[i].rssi, 10);
          DEBUG_PRINT(F("INFO: Tracked device discovered, Address: "));
          DEBUG_PRINT(address);
          DEBUG_PRINT(F(", RSSI: "));
          DEBUG_PRINTLN(advertisedDevice.getRSSI());
        }
        findedAdvertisedDevice = true;
        break;
      }
    }

    if (!findedAdvertisedDevice)
    {
      NB_OF_BLE_DISCOVERED_DEVICES = NB_OF_BLE_DISCOVERED_DEVICES + 1;
      BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].address = address;
      BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].isDiscovered = true;
      BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].lastDiscovery = millis();
      itoa(advertisedDevice.getRSSI(), BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].rssi, 10);

      DEBUG_PRINT(F("INFO: Device discovered, Address: "));
      DEBUG_PRINT(advertisedDevice.getAddress().toString().c_str());
      DEBUG_PRINT(F(", RSSI: "));
    }
  }
};

static BLEUUID service_BATT_UUID(BLEUUID((uint16_t)0x180F));
static BLEUUID char_BATT_UUID(BLEUUID((uint16_t)0x2A19));

#if 0
bool batteryLevel(const String& address, String &batteryLevel) 
{
    std::ostringstream osAddress;
    int len = address.length();
    for(int i= 0;i < len;i++)
    { 
       osAddress << address[i];
       if( (i & 1) == 1  && i != len-1)
          osAddress << ':';
    }

    BLEAddress* pAddress = new BLEAddress(osAddress.str());
    Serial.print("connecting to : ");
    Serial.println(pAddress->toString().c_str());
    // create a new client
    BLEClient*  pClient;
    pClient  = BLEDevice::createClient();
    Serial.println("Created client");

    // Connect to the remove BLE Server.
    pClient->connect(*pAddress);
    Serial.println("Connected to server");
	BLERemoteService* pRemote_BATT_Service = pClient->getService(service_BATT_UUID);
	if (pRemote_BATT_Service == nullptr) {
		Serial.print("Failed to find BATT service : ");
		//Serial.println(service_BATT_UUID.toString().c_str());
		return false;
    }
	BLERemoteCharacteristic* pRemote_BATT_Characteristic = pRemote_BATT_Service->getCharacteristic(char_BATT_UUID);
	if (pRemote_BATT_Characteristic == nullptr) {
		Serial.print("Failed to find BATT characteristic : ");
		//Serial.println(char_BATT_UUID.toString().c_str());
		return false;
	}
    std::string value = pRemote_BATT_Characteristic->readValue();
    batteryLevel = value.c_str();
    pClient->disconnect();
    delete pClient;
    delete pAddress;
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
}

void publishBLEState(String address, const char *state, const char *rssi)
{
  String baseTopic = MQTT_BASE_SENSOR_TOPIC;
  baseTopic += "/" + address;
  String stateTopic = baseTopic + "/state";
  String rssiTopic = baseTopic + "/rssi";
  publishToMQTT(stateTopic.c_str(), state, false);
  publishToMQTT(rssiTopic.c_str(), rssi, false);
  std::ostringstream payload;
  payload << "{ \"state\":\"" << state << "\",\"rssi\":" << rssi << "}";
  publishToMQTT(baseTopic.c_str(), payload.str().c_str(), false);

#if 0
  String battLevel;
  if(batteryLevel(address,battLevel))
  {
    DEBUG_PRINT("batteryLevel ");
    DEBUG_PRINT(address.c_str());
    DEBUG_PRINTLN(battLevel.c_str());
  }
  #endif
}

void publishSySInfo()
{
  String baseTopic = MQTT_BASE_SENSOR_TOPIC;
  String sysTopic = baseTopic + "/sysinfo";
  std::ostringstream payload;
  payload << "{ \"uptime\":\"" << millis() << "\",\"version\":" << VERSION << ",\"SSID\":\"" << WIFI_SSID <<"\"}";
  publishToMQTT(sysTopic.c_str(), payload.str().c_str(), false);
}

void loop()
{
  if(lastSySInfoTime == 0)
  {
    lastSySInfoTime = millis();
  }

  mqttClient.loop();
  timerWrite(timer, 0); //reset timer (feed watchdog)
  long tme = millis();
  Serial.println("INFO: Running mainloop");
  DEBUG_PRINT("Number device discovered: ");
  DEBUG_PRINTLN(NB_OF_BLE_DISCOVERED_DEVICES);

  if (NB_OF_BLE_DISCOVERED_DEVICES > 90)
  {
    DEBUG_PRINTLN("INFO: Riavvio perché ho finito l'array\n");
    esp_restart();
  }
  pBLEScan->setActiveScan(true);
  pBLEScan->start(BLE_SCANNING_PERIOD);

  for (uint8_t i = 0; i < NB_OF_BLE_DISCOVERED_DEVICES; i++)
  {
    if (BLETrackedDevices[i].isDiscovered == true && BLETrackedDevices[i].lastDiscovery + MAX_NON_ADV_PERIOD < millis())
    {
      BLETrackedDevices[i].isDiscovered = false;
    }
  }

  for (uint8_t i = 0; i < NB_OF_BLE_DISCOVERED_DEVICES; i++)
  {
    if (BLETrackedDevices[i].isDiscovered)
    {
      publishBLEState(BLETrackedDevices[i].address, MQTT_PAYLOAD_ON, BLETrackedDevices[i].rssi);
    }
    else
    {
      publishBLEState(BLETrackedDevices[i].address, MQTT_PAYLOAD_OFF, "-100");
    }
  }

  //System Information
  if((lastSySInfoTime + SYS_INFORMATION_DELAY) < millis())
  {
      publishSySInfo();
      lastSySInfoTime = millis();
  }

  connectToMQTT();
}
