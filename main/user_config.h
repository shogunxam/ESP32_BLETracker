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
// Each entry uses the format:
// { "DEVICE-ID", readBattery, "Description" }
// Example:
// { "A6B5C4D3E2F1", true, "My iTag" }
//
// DEVICE-ID can be either:
// - The device MAC address, represented as a 12-character uppercase hexadecimal string
//   without separators (e.g. "A6B5C4D3E2F1"), or
// - A 32-byte UUID represented as a 40-character uppercase hexadecimal string without
//   separators, where the last 4 bytes encode the beacon Major and Minor values.
//
// Example:
// "0102030405060708090A0B0C0D0E0F1000010002"
// represents a beacon UUID with Major = 1 and Minor = 2.

//Each block is coma separated
//In example
//#define BLE_KNOWN_DEVICES_LIST  {"0102030405060708090A0B0C0D0E0F1000010002", false, "iBeacon"},{"AABBCCDDEEFF", true, "Nut"}, {"A1B2C3D4E5F6", false, "iTag"}, {"1A2B3C4D5E6F", false, ""}
#define BLE_KNOWN_DEVICES_LIST  

//NTP Server configurations
#define NTP_SERVER          "pool.ntp.org"

//If empty time zone is autodetected using the web service http://ip-api.com/json
//To set it manually get your time zone from https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIME_ZONE ""

#endif
