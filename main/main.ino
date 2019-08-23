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

typedef struct {
  String  address;
  char    rssi[4];
  bool    isDiscovered;
  long    lastDiscovery;
  bool    toNotify;
  char    mqttTopic[48];
  char    rssiTopic[48];
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
#define     DEBUG_PRINT(x)    Serial.print(x)
#define     DEBUG_PRINTLN(x)  Serial.println(x)
#else
#define     DEBUG_PRINT(x)
#define     DEBUG_PRINTLN(x)
#endif

BLEScan*      pBLEScan;
WiFiClient    wifiClient;
PubSubClient  mqttClient(wifiClient);


///////////////////////////////////////////////////////////////////////////
//   MQTT
///////////////////////////////////////////////////////////////////////////
volatile unsigned long lastMQTTConnection = 0;
char MQTT_CLIENT_ID[7] = {0};
char MQTT_AVAILABILITY_TOPIC[sizeof(MQTT_AVAILABILITY_TOPIC_TEMPLATE) - 2] = {0};
/*
  Function called to publish to a MQTT topic with the given payload
*/
void publishToMQTT(char* p_topic, const char* p_payload, bool p_retain) {
  while (!mqttClient.connected()) {
    DEBUG_PRINT(F("INFO: Connecting to MQTT broker: "));
    DEBUG_PRINTLN(MQTT_SERVER);
    connectToMQTT();
    delay(500);
  }

  if (mqttClient.publish(p_topic, p_payload, p_retain)) {
    DEBUG_PRINT(F("INFO: MQTT message published successfully, topic: "));
    DEBUG_PRINT(p_topic);
    DEBUG_PRINT(F(", payload: "));
    DEBUG_PRINT(p_payload);
    DEBUG_PRINT(F(", retain: "));
    DEBUG_PRINTLN(p_retain);
  } else {
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
void connectToMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINT(F("INFO: WiFi connecting to: "));
    DEBUG_PRINTLN(WIFI_SSID);
    delay(10);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    randomSeed(micros());

    while (WiFi.status() != WL_CONNECTED) {
      DEBUG_PRINT(F("."));
      delay(500);
    }
    Serial.println("Connected to WiFi.");
    DEBUG_PRINT("Current IP: ");
    DEBUG_PRINTLN(WiFi.localIP());
  }

  memset(MQTT_CLIENT_ID, 0, sizeof(MQTT_CLIENT_ID));
  sprintf(MQTT_CLIENT_ID, "%06X", ESP.getEfuseMac());

  memset(MQTT_AVAILABILITY_TOPIC, 0, sizeof(MQTT_AVAILABILITY_TOPIC));
  sprintf(MQTT_AVAILABILITY_TOPIC, MQTT_AVAILABILITY_TOPIC_TEMPLATE,LOCATION);
  DEBUG_PRINT(F("INFO: MQTT availability topic: "));
  DEBUG_PRINTLN(MQTT_AVAILABILITY_TOPIC);

  if (!mqttClient.connected()) {
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, MQTT_AVAILABILITY_TOPIC, 1, true, MQTT_PAYLOAD_UNAVAILABLE)) {
      DEBUG_PRINTLN(F("INFO: The client is successfully connected to the MQTT broker"));
      publishToMQTT(MQTT_AVAILABILITY_TOPIC, MQTT_PAYLOAD_AVAILABLE, true);
    } else {
      DEBUG_PRINTLN(F("ERROR: The connection to the MQTT broker failed"));
      DEBUG_PRINT(F("INFO: MQTT username: "));
      DEBUG_PRINTLN(MQTT_USERNAME);
      DEBUG_PRINT(F("INFO: MQTT password: "));
      DEBUG_PRINTLN(MQTT_PASSWORD);
      DEBUG_PRINT(F("INFO: MQTT broker: "));
      DEBUG_PRINTLN(MQTT_SERVER);
    }
  } else {
    if (lastMQTTConnection < millis()) {
      lastMQTTConnection = millis() + MQTT_CONNECTION_TIMEOUT;
      publishToMQTT(MQTT_AVAILABILITY_TOPIC, MQTT_PAYLOAD_AVAILABLE, true);
    }
  }
}

hw_timer_t *timer = NULL;
void IRAM_ATTR resetModule() {
  ets_printf("INFO: Reboot\n");
  esp_restart();
}

///////////////////////////////////////////////////////////////////////////
//   BLUETOOTH
///////////////////////////////////////////////////////////////////////////
class MyAdvertisedDeviceCallbacks:
  public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      bool findedAdvertisedDevice = false;
      String address(advertisedDevice.getAddress().toString().c_str());
      address.replace(":", "");
      address.toUpperCase();
      for (uint8_t i = 0; i < NB_OF_BLE_DISCOVERED_DEVICES; i++) {
        if (address == BLETrackedDevices[i].address) {
          if (!BLETrackedDevices[i].isDiscovered) {
            BLETrackedDevices[i].isDiscovered = true;
            BLETrackedDevices[i].lastDiscovery = millis();
            BLETrackedDevices[i].toNotify = true;
            itoa(advertisedDevice.getRSSI(), BLETrackedDevices[i].rssi, 10);

            DEBUG_PRINT(F("INFO: Tracked device newly discovered, Address: "));
            DEBUG_PRINT(address);
            DEBUG_PRINT(F(", RSSI: "));
            DEBUG_PRINTLN(advertisedDevice.getRSSI());
          } else {
            BLETrackedDevices[i].toNotify = true;
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
      if (!findedAdvertisedDevice) {
        NB_OF_BLE_DISCOVERED_DEVICES = NB_OF_BLE_DISCOVERED_DEVICES + 1;
        BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].address = address;
        BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].isDiscovered = true;
        BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].lastDiscovery = millis();
        BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].toNotify = false;
        itoa(advertisedDevice.getRSSI(), BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].rssi, 10);

        memset(MQTT_CLIENT_ID, 0, sizeof(MQTT_CLIENT_ID));
        sprintf(MQTT_CLIENT_ID, "%06X", ESP.getEfuseMac());

        char tmp_mqttTopic[sizeof(MQTT_SENSOR_TOPIC_TEMPLATE) + sizeof(LOCATION) + 12 - 4] = {0};
        char tmp_ble_address[13] = {0};
        address.toCharArray(tmp_ble_address, sizeof(tmp_ble_address));
        sprintf(tmp_mqttTopic, MQTT_SENSOR_TOPIC_TEMPLATE, LOCATION, tmp_ble_address);
        memcpy(BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].mqttTopic, tmp_mqttTopic, sizeof(tmp_mqttTopic) + 1);

        char tmp_rssiTopic[sizeof(MQTT_RSSI_TOPIC_TEMPLATE) + sizeof(LOCATION) + 12 - 4] = {0};
        sprintf(tmp_rssiTopic, MQTT_RSSI_TOPIC_TEMPLATE, LOCATION, tmp_ble_address);
        memcpy(BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].rssiTopic, tmp_rssiTopic, sizeof(tmp_rssiTopic) + 1);

        DEBUG_PRINT(F("INFO: Device discovered, Address: "));
        //DEBUG_PRINT(advertisedDevice.getAddress().toString().c_str());
        DEBUG_PRINT(address);
        DEBUG_PRINT(F(", RSSI: "));
        DEBUG_PRINT(advertisedDevice.getRSSI());
        DEBUG_PRINT(F(", MQTT sensor topic: "));
        DEBUG_PRINT(BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].mqttTopic);
        DEBUG_PRINT(F(", RSSI sensor topic: "));
        DEBUG_PRINTLN(BLETrackedDevices[NB_OF_BLE_DISCOVERED_DEVICES - 1].rssiTopic);
      }
    }
};

///////////////////////////////////////////////////////////////////////////
//   SETUP() & LOOP()
///////////////////////////////////////////////////////////////////////////
void setup() {
#if defined(DEBUG_SERIAL)
  Serial.begin(115200);
#endif
  Serial.println("INFO: Running setup");

  timer = timerBegin(0, 80, true); //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);
  timerAlarmWrite(timer, 120000000, false); //set time in us 120000000 = 120 sec
  timerAlarmEnable(timer); //enable interrupt

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(false);

  mqttClient.setServer(MQTT_SERVER, MQTT_SERVER_PORT);
}

void loop() {
  mqttClient.loop();
  timerWrite(timer, 0); //reset timer (feed watchdog)
  long tme = millis();
  Serial.println("INFO: Running mainloop");
  DEBUG_PRINT("Number device discovered: ");
  DEBUG_PRINTLN(NB_OF_BLE_DISCOVERED_DEVICES);

  if (NB_OF_BLE_DISCOVERED_DEVICES > 90) {
    DEBUG_PRINTLN("INFO: Riavvio perché ho finito l'array\n");
    esp_restart();
  }

  pBLEScan->start(BLE_SCANNING_PERIOD);

  for (uint8_t i = 0; i < NB_OF_BLE_DISCOVERED_DEVICES; i++) {
    if (BLETrackedDevices[i].isDiscovered == true && BLETrackedDevices[i].lastDiscovery + MAX_NON_ADV_PERIOD < millis()) {
      BLETrackedDevices[i].isDiscovered = false;
      BLETrackedDevices[i].toNotify = true;
    }
  }

  for (uint8_t i = 0; i < NB_OF_BLE_DISCOVERED_DEVICES; i++) {
    //    if (BLETrackedDevices[i].toNotify) {
    if (BLETrackedDevices[i].isDiscovered) {
      publishToMQTT(BLETrackedDevices[i].mqttTopic, MQTT_PAYLOAD_ON, false);
      publishToMQTT(BLETrackedDevices[i].rssiTopic, BLETrackedDevices[i].rssi, false);
      std::ostringstream payload;
      payload << "{ \"state\" : \"" << MQTT_PAYLOAD_ON << "\", \"rssi\" : \"" << BLETrackedDevices[i].rssi << "\" }";
      char BaseTopic[sizeof(MQTT_BASE_SENSOR_TOPIC_TEMPLATE) + sizeof(LOCATION) + 12 - 4] = {0};
      sprintf(BaseTopic,MQTT_BASE_SENSOR_TOPIC_TEMPLATE,LOCATION,BLETrackedDevices[i].address.c_str());
      publishToMQTT(BaseTopic, payload.str().c_str(), false);

    } else {
      publishToMQTT(BLETrackedDevices[i].mqttTopic, MQTT_PAYLOAD_OFF, false);
      publishToMQTT(BLETrackedDevices[i].rssiTopic, "-100", false);
      std::ostringstream payload;
      payload << "{ \"state\" : \"" << MQTT_PAYLOAD_OFF << "\", \"rssi\" : '-100\" }";
      char BaseTopic[sizeof(MQTT_BASE_SENSOR_TOPIC_TEMPLATE) + sizeof(LOCATION) + 12 - 4] = {0};
      sprintf(BaseTopic,MQTT_BASE_SENSOR_TOPIC_TEMPLATE,LOCATION,BLETrackedDevices[i].address.c_str());
      publishToMQTT(BaseTopic, payload.str().c_str(), false);
      BLETrackedDevices[i].toNotify = false;
    }
  }

  //  }
  connectToMQTT();

  //mqttClient.disconnect();
  //WiFi.mode(WIFI_OFF);
}
