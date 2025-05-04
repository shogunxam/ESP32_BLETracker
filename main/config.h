#ifndef CONFIG_ESP32_BLETRACKER
#define CONFIG_ESP32_BLETRACKER
///////////////////////////////////////////////////////////////////////////
//  CONFIGURATION - SOFTWARE
///////////////////////////////////////////////////////////////////////////

#include "user_config.h"

//Bluetooth scans can either be passive or active. 
//When passively scanning, a device will only listen to Bluetooth devices, quietly collecting data about its surroundings.
//Active scanners respond to every device they hear from, asking if they have more data to send.
//Generally, passive scans are enough and use less power than active scans, so are the best option.
//Active scans are only necessary if you encounter strange issues i.e. the Tracker discovers devices when they are not present.
#define ACTIVE_SCAN     false

//Scanning to discover devices is performed every BLE_SCANNING_PERIOD seconds
#define BLE_SCANNING_PERIOD   10    /*10 s*/

//If the device is not advertides for MAX_NON_ADV_PERIOD seconds it's considered away
#define MAX_NON_ADV_PERIOD    120 /*2m -> 2m*60s*/

//The device's battery is read every BATTERY_READ_PERIOD seconds
//If the battery value's is not read after BATTERY_READ_PERIOD it becomes unknown
#define BATTERY_READ_PERIOD    43200 /*12h -> 12h*60m*60s*/

//In case of failure reading the battery's value we retry after BATTERY_RETRY_PERIOD seconds
#define BATTERY_RETRY_PERIOD    3600 /*1h -> 60m*60s*/

//Retries to read the battery level if connections fails before to give up until BATTERY_RETRY_PERIOD expires
#define MAX_BLE_CONNECTION_RETRIES 3

// Debug output
//#define DEBUG_SERIAL


#define PUBLISH_BATTERY_LEVEL       true

// MQTT
#ifndef USE_MQTT
#define USE_MQTT true
#endif

#if USE_MQTT
#define  SERVER_USERNAME MQTT_USERNAME
#define  SERVER_PASSWORD MQTT_PASSWORD
#define  SERVER_ADDRESS  MQTT_SERVER
#define  SERVER_PORT     MQTT_SERVER_PORT  
#endif 

//If the MQTT_CONNECTION_TIME_OUT is expired the availability topic is re-published
#define MQTT_CONNECTION_TIME_OUT 5 // [seconds]

// MQTT availability: available/unavailable
//#define MQTT_BASE_SENSOR_TOPIC     LOCATION "/" GATEWAY_NAME
//#define MQTT_AVAILABILITY_TOPIC    MQTT_BASE_SENSOR_TOPIC "/LWT"

#define MQTT_PAYLOAD_ON   "on"
#define MQTT_PAYLOAD_OFF  "off"

#define MQTT_PAYLOAD_AVAILABLE    "online"
#define MQTT_PAYLOAD_UNAVAILABLE  "offline"

#define PUBLISH_SIMPLE_JSON         true
#define PUBLISH_SEPARATED_TOPICS    false

#define ENABLE_HOME_ASSISTANT_MQTT_DISCOVERY true
///////////////////


//FHEM LE Presence server
#ifndef USE_FHEM_LEPRESENCE_SERVER
#define USE_FHEM_LEPRESENCE_SERVER false
#endif
/////////////////////////

#ifndef USE_UDP
#define USE_UDP false
#endif

#if USE_UDP
#define  SERVER_ADDRESS  UDP_SERVER
#define  SERVER_PORT     UDP_SERVER_PORT  
#endif 

#if USE_FHEM_LEPRESENCE_SERVER
#define PROGRESSIVE_SCAN true
#else
#define PROGRESSIVE_SCAN false
#endif


#ifndef  SERVER_USERNAME
#define  SERVER_USERNAME ""
#endif
#ifndef  SERVER_PASSWORD
#define  SERVER_PASSWORD ""
#endif
#ifndef  SERVER_ADDRESS
#define  SERVER_ADDRESS ""
#endif
#ifndef  SERVER_PORT
#define  SERVER_PORT  0
#endif



#define ENABLE_OTA_WEBSERVER    true

#define WIFI_CONNECTION_TIME_OUT  30 /*30 seconds*/

//Print more data in the System Info page
#ifndef DEVELOPER_MODE
#define DEVELOPER_MODE false
#endif

//Erase all the persistent data at the first execution just after the new firmware is uploaded
#define ERASE_DATA_AFTER_FLASH false

//Enable persistent logs on File System
#define ENABLE_FILE_LOG true
#define DEFAULT_FILE_LOG_LEVEL 2 /*Error = 0, Warning = 1, Info = 2, Debug = 3, Verbose = 4*/
#define MAX_NUM_OF_SAVED_LOGS 200

//Set the number of advertisement for the same device to detect during a scan
//to consider the device as discovered
#define NUM_OF_ADVERTISEMENT_IN_SCAN 1


#define COUNT_ENABLED(a, b, c) ( a + b + c)

#if COUNT_ENABLED(USE_MQTT, USE_FHEM_LEPRESENCE_SERVER, USE_UDP) > 1
  #error Module conflict: Only one of MQTT, FHEM LE Presence Server or UDP can be enabled at a time
#endif

#endif /*CONFIG_ESP32_BLETRACKER*/
