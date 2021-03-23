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

//List of known devices you want track
//Each entry is in the format {"MAC-ADDRESS", read-battery, "Description"} i.e.{"A6B5C4D3E2F1", true, "My iTag"}
//Mac Addresses are in the form "A6B5C4D3E2F1" (uppercase and no separator)
//Each block is coma separated
#define BLE_KNOWN_DEVICES_LIST  {"AABBCCDDEEFF", true, "Nut"}, {"A1B2C3D4E5F6", false, "iTag"}, {"1A2B3C4D5E6F", false, ""}

//NTP Server configurations
#define NTP_SERVER          "pool.ntp.org"
//Get here your time zone https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIME_ZONE "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"

#endif
