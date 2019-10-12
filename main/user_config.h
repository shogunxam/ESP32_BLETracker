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

#define OTA_USER    "admin"
#define OTA_PASSWORD "admin"

//List of MAC Address devices for which to return the battery level
//Mac Addresses are in the form "A6B5C4D3E2F1" and coma separated
//Undef or comment this line to check all the advertised devices
#define BLE_BATTERY_WHITELIST  "A6B5C4D3E2F1","F6E5D4C3B2A1"

#endif
