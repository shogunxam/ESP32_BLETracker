#ifndef _WIFI_MANAGER_H_
#define _WIFI_MANAGER_H_
#include <WiFi.h>

enum class WiFiMode
{
    Initializing = 0,
    AcessPoint = 1,
    Station = 2
};

void StartAccessPointMode();
bool WiFiConnect(const String& _ssid_, const String& _password_) ;
WiFiMode GetWifiMode();
bool IsAccessPointModeOn();
void CheckAPModeTimeout();

extern WiFiClient wifiClient;
#endif /*_WIFI_MANAGER_H_*/