///////////////////////////////////////////////////////////////////////////
//  CONFIGURATION - SOFTWARE
///////////////////////////////////////////////////////////////////////////
#ifndef CONFIG_ESP32_BLETRACKER
#define CONFIG_ESP32_BLETRACKER

#define BLE_SCANNING_PERIOD   10    /*10 s*/
#define MAX_NON_ADV_PERIOD    120000 /*2m*/
#define MAX_BATTERY_READ_PERIOD    3600000 /*1h*/
//Retries to read the battery level if connections fails before to give up until MAX_BATTERY_READ_PERIOD expires
#define MAX_BLE_CONNECTION_RETRIES 3
// Location of the BLE scanner
#define LOCATION "home"

// Debug output
#define DEBUG_SERIAL

// Wi-Fi credentials
#define WIFI_SSID     "XXXXXXXXXX"
#define WIFI_PASSWORD "XXXXXXXXXX"

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

#define PUBLISH_SIMPLE_JSON         true
#define PUBLISH_SEPARATED_TOPICS    false
#define PUBLISH_BATTERY_LEVEL       true

//Replace using correct MAC Address values or undef
#define BLE_BATTERY_WHITELIST       "XXXXXXXXX","YYYYYYYY"

#define ENABLE_OTA_WEBSERVER    true

#endif /*CONFIG_ESP32_BLETRACKER*/
