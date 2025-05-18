#ifndef _WIFI_MANAGER_H_
#define _WIFI_MANAGER_H_
#include <WiFi.h>
#include <functional>

namespace WiFiManager
{
    enum class WiFiMode
    {
        Initializing = 0,
        AcessPoint = 1,
        Station = 2
    };

    void AddWiFiConnectedCallback(std::function<void(void)> callback);
    void StartAccessPointMode();
    bool WiFiConnect(const String &_ssid_, const String &_password_);
    WiFiMode GetWifiMode();
    bool IsAccessPointModeOn();
    void CheckAPModeTimeout();
    const char *GetDefaultHostName();    // Returns ESP32-XXXXXXXXXXXX
    const char *GetDefaultGatewayName(); // Returns GATEWAY_NAME if set, otherwise returns ESP32-XXXXXXXXXXXX

    WiFiClient& GetWiFiClient();
}
#endif /*_WIFI_MANAGER_H_*/