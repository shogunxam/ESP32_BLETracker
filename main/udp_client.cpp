#include "udp_client.h"
#if USE_UDP
#include <WiFiUdp.h>
#include "settings.h"
#include "DebugPrint.h"
#include "WiFiManager.h"

#define UDP_PORT 1883
namespace UDPClient
{
    static WiFiUDP udpClient;

    void initializeUDP()
    {
        udpClient.begin(UDP_PORT);
        DEBUG_PRINTLN("UDP client started");
    }

    void publishBLEState(const BLETrackedDevice &device)
    {

        char payload[150];
        int rssi = device.isDiscovered ? device.rssiValue : -100;
        unsigned long now = NTPTime::getTimeStamp();

#if PUBLISH_BATTERY_LEVEL
        snprintf(payload, sizeof(payload), "module=%s.%s %s=%d rssi=%d battery=%d timestamp=%lu",
                 LOCATION, SettingsMngr.gateway.c_str(), device.address, device.isDiscovered, device.rssiValue, device.batteryLevel, now);
#else
        snprintf(payload, sizeof(payload), "module=%s.%s %s=%s rssi=%d timestamp=%lu",
                 LOCATION, SettingsMngr.gateway.c_str(), device.address, device.isDiscovered, device.rssiValue, now);
#endif

        udpClient.beginPacket(SettingsMngr.serverAddr.c_str(), SettingsMngr.serverPort);
        udpClient.write((uint8_t *)payload, strlen(payload));
        udpClient.endPacket();
        DEBUG_PRINTLN(payload);
    }

    void publishSySInfo()
    {
        const size_t ssidlen = SettingsMngr.wifiSSID.length() + 1;
        unsigned long now = NTPTime::getTimeStamp();
        long rssi = WiFi.RSSI();
        static String IP;
        IP.reserve(16);
        IP = WiFi.localIP().toString();

        char strmilli[20];

        const size_t maxPayloadSize = ssidlen + 200;
        char payload[maxPayloadSize];

        uint8_t mac[6];
        WiFi.macAddress(mac);
        snprintf(payload, maxPayloadSize, "module=%s.%s alive uptime=%s SSID=%s rssi=%d IP=%s MAC=%02X:%02X:%02X:%02X:%02X:%02X timestamp=%lu",
                 LOCATION, SettingsMngr.gateway.c_str(), formatMillis(millis(), strmilli), SettingsMngr.wifiSSID.c_str(), rssi, IP.c_str(),
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], now);

        udpClient.beginPacket(SettingsMngr.serverAddr.c_str(), SettingsMngr.serverPort);
        udpClient.write((uint8_t *)payload, strlen(payload));
        udpClient.endPacket();
        DEBUG_PRINTLN(payload);
    }
}
#endif // USE_UDP