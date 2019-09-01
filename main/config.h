///////////////////////////////////////////////////////////////////////////
//  CONFIGURATION - SOFTWARE
///////////////////////////////////////////////////////////////////////////
uint8_t NB_OF_BLE_DISCOVERED_DEVICES = 0;
BLETrackedDevice BLETrackedDevices[99] = {};

#define BLE_SCANNING_PERIOD   10
#define MAX_NON_ADV_PERIOD    120000

// Location of the BLE scanner
#define LOCATION "home"

// Debug output
#define DEBUG_SERIAL

// Wi-Fi credentials
#define WIFI_SSID     "XXXXXXXXXX"
#define WIFI_PASSWORD "XXXXXXXXXX"

// Over-the-Air update
// Not implemented yet
//#define OTA
//#define OTA_HOSTNAME  ""    // hostname esp8266-[ChipID] by default
//#define OTA_PASSWORD  ""    // no password by default
//#define OTA_PORT      8266  // port 8266 by default

// MQTT
#define MQTT_USERNAME     "XXXXXXXXXX"
#define MQTT_PASSWORD     "XXXXXXXXXX"
#define MQTT_SERVER       "XXX.XXX.XXX.XXX"
#define MQTT_SERVER_PORT  1883

#define MQTT_CONNECTION_TIMEOUT 5000 // [ms]

#ifndef GATEWAY_NAME
#define GATEWAY_NAME "BLETracker"
#endif

// MQTT availability: available/unavailable
#define MQTT_BASE_SENSOR_TOPIC     LOCATION "/" GATEWAY_NAME
#define MQTT_AVAILABILITY_TOPIC    MQTT_BASE_SENSOR_TOPIC "/LWT"

#define MQTT_PAYLOAD_ON   "on"
#define MQTT_PAYLOAD_OFF  "off"

#define MQTT_PAYLOAD_AVAILABLE    "online"
#define MQTT_PAYLOAD_UNAVAILABLE  "offline"
