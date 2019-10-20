#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_

#define LOCATION "home"
#define GATEWAY_NAME "BLETracker"

// Wi-Fi credentials
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// MQTT
#define MQTT_USERNAME     "YOUR_MQTT_USERNAME"
#define MQTT_PASSWORD     "YOUR_MQTT_PASSWORD"
#define MQTT_SERVER       "YOUR_MQTT_SERVER_IP_ADDRESS"
#define MQTT_SERVER_PORT  1883

#define WEBSERVER_USER        "admin"
#define WEBSERVER_PASSWORD    "admin"

//List of devices you want track
//Replace using correct MAC Address values or undef/comment to track all the advertised devices
//Mac Addresses are in the form "A6B5C4D3E2F1" (uppercase and no separator) and coma separated
//#define BLE_TRACKER_WHITELIST  "A6B5C4D3E2F1","F6E5D4C3B2A1","54FA8BC0467C"

//List of devices for which you want to know the battery level.
//Replace using correct MAC Address values or undef/comment to read the battery level of all the tracked devices
//Mac Addresses are in the form "A6B5C4D3E2F1" (uppercase and no separator) and coma separated
#define BLE_BATTERY_WHITELIST  "A6B5C4D3E2F1","F6E5D4C3B2A1"

#endif
