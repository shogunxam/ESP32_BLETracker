#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_

#define LOCATION "home"
#define GATEWAY_NAME "BLETracker"

// Wi-Fi credentials
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

/*Set to false to assign manually an IP to the BLETracker*/
#define USE_DHCP true
#if !USE_DHCP
#define LOCAL_IP        "192.168.0.100" /*IP to assign to the BLETRacker, it must be unique in your network*/
#define NETMASK         "255.255.255.0" /*Usually you don't need to change this*/
#define GATEWAY         "192.168.0.1"   /*IP of your router with access to internet*/
#define PRIMARY_DNS     "8.8.8.8"       /*Optional*/
#define SECONDARY_DNS   "8.8.4.4"       /*Optional*/
#endif

// MQTT
#define MQTT_USERNAME     "YOUR_MQTT_USERNAME"
#define MQTT_PASSWORD     "YOUR_MQTT_PASSWORD"
#define MQTT_SERVER       "YOUR_MQTT_SERVER_IP_ADDRESS"
#define MQTT_SERVER_PORT  1883

#define WEBSERVER_USER        "admin"
#define WEBSERVER_PASSWORD    "admin"

//Set to true if you want track only the devices in the white list
#define ENABLE_BLE_TRACKER_WHITELIST true

//List of devices you want track
//Replace using correct MAC Address values or undef/comment to track all the advertised devices
//Mac Addresses are in the form "A6B5C4D3E2F1" (uppercase and no separator) and coma separated
//#define BLE_TRACKER_WHITELIST  "A6B5C4D3E2F1","F6E5D4C3B2A1","54FA8BC0467C"

//List of devices for which you want to know the battery level.
//Replace using correct MAC Address values or undef/comment to read the battery level of all the tracked devices
//Mac Addresses are in the form "A6B5C4D3E2F1" (uppercase and no separator) and coma separated
#define BLE_BATTERY_WHITELIST  "A6B5C4D3E2F1","F6E5D4C3B2A1"

//NTP Server configurations
#define NTP_SERVER          "pool.ntp.org"
//Time Offset in seconds from GMT i.e. TZ+1 = 3600, TZ-1 = -3600
#define GMT_OFFSET_IN_SEC   3600 
//Daylight saving time offset in seconds 
#define DST_OFFSET_INSEC    3600

#endif
