#ifndef CONFIG_ESP32_BLETRACKER
#define CONFIG_ESP32_BLETRACKER
///////////////////////////////////////////////////////////////////////////
//  CONFIGURATION - SOFTWARE
///////////////////////////////////////////////////////////////////////////

#include "user_config.h"

//Scanning to discover devices is performed every BLE_SCANNING_PERIOD seconds
#define BLE_SCANNING_PERIOD   10    /*10 s*/

//If the device is not advertides for MAX_NON_ADV_PERIOD milliseconds it's considered away
#define MAX_NON_ADV_PERIOD    120000 /*2m -> 2m*60s*1000milli*/

//The device's battery is read every BATTERY_READ_PERIOD milliseconds
//If the battery value's is not read after BATTERY_READ_PERIOD it becomes unknown
#define BATTERY_READ_PERIOD    43200000 /*12h -> 12h*60m*60s*1000milli*/

//In case of failure reading the battery's value we retry after BATTERY_RETRY_PERIOD milliseconds
#define BATTERY_RETRY_PERIOD    3600000 /*1h -> 60m*60s*1000milli*/

//Retries to read the battery level if connections fails before to give up until BATTERY_RETRY_PERIOD expires
#define MAX_BLE_CONNECTION_RETRIES 3

// Debug output
//#define DEBUG_SERIAL

// MQTT
#define MQTT_CONNECTION_TIME_OUT 5000 // [ms]

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

#define ENABLE_OTA_WEBSERVER    true

#define WIFI_CONNECTION_TIME_OUT  30000 /*30 seconds*/

//Print more data in the System Info page
#ifndef DEVELOPER_MODE
#define DEVELOPER_MODE false
#endif

//Erase all the persistent data at the first execution just after the new firmware is uploaded
#define ERASE_DATA_AFTER_FLASH false

//Enable persistent logs on File System
#define ENABLE_FILE_LOG true
#define MAX_NUM_OF_SAVED_LOGS 200
#endif /*CONFIG_ESP32_BLETRACKER*/
