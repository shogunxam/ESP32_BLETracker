#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_

// Location name maximum length is 32 characters
#define LOCATION "home"
// Gateway name maximum length is 32 characters. If empty, the device will use the MAC address as gateway name
#define GATEWAY_NAME ""

// Wi-Fi credentials
// NOTE: If you set them the WiFi credential AcessPoint Mode will not work and BLETracker will connect always to this WiFi
#define WIFI_SSID     ""
#define WIFI_PASSWORD ""

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
#define MQTT_USERNAME     "" /*Your MQTT username*/
#define MQTT_PASSWORD     "" /*Your MQTT password*/
#define MQTT_SERVER       "" /*Your MQTT server address*/
#define MQTT_SERVER_PORT  1883

//UDP
#define UDP_SERVER        "" /*IP of the UDP server*/
#define UDP_SERVER_PORT   1234

#define WEBSERVER_USER        "admin"
#define WEBSERVER_PASSWORD    "admin"

//Set to true if you want track only the devices in the white list
#define ENABLE_BLE_TRACKER_WHITELIST true

//List of known devices you want track
//Each entry is in the format {"MAC-ADDRESS", read-battery, "Description"} i.e.{"A6B5C4D3E2F1", true, "My iTag"}
//Mac Addresses are in the form "A6B5C4D3E2F1" (uppercase and no separator)
//Each block is coma separated
//In example
//#define BLE_KNOWN_DEVICES_LIST  {"AABBCCDDEEFF", true, "Nut"}, {"A1B2C3D4E5F6", false, "iTag"}, {"1A2B3C4D5E6F", false, ""}
#define BLE_KNOWN_DEVICES_LIST  

//NTP Server configurations
#define NTP_SERVER          "pool.ntp.org"

//If empty time zone is autodetected using the web service http://ip-api.com/json
//To set it manually get your time zone from https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIME_ZONE ""

#endif
