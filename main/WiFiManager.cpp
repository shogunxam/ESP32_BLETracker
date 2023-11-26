#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "config.h"
#include "WiFiManager.h"
#include "DebugPrint.h"
extern "C" {
  #include <esp_wifi.h>
}
#include "NTPTime.h"
#include "SPIFFSLogger.h"
#include "settings.h"

WiFiClient wifiClient;
static WiFiMode gWiFiMode = WiFiMode::Initializing;

bool IsAccessPointModeOn()
{
  return gWiFiMode == WiFiMode::AcessPoint;
}

WiFiMode GetWifiMode()
{
  return gWiFiMode;
}

void StartAccessPointMode()
{
    // Enable Access Point Mode
    DEBUG_PRINT("Starting AccessPoint Mode...");
    IPAddress local_ip(192,168,2,1);
    IPAddress gateway(192,168,2,1);
    IPAddress subnet(255,255,255,0);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAP("ESP32_BLETRACKER", nullptr);
    gWiFiMode = WiFiMode::AcessPoint;
    DEBUG_PRINT("AccessPoint Mode enabled.");
}

void WiFiConnect(const String &_ssid_, const String &_password_)
{
  if (WiFi.status() != WL_CONNECTED && !_ssid_.isEmpty() && gWiFiMode != WiFiMode::AcessPoint)
  {
    DEBUG_PRINTF("Connecting to WiFi %s...", _ssid_.c_str());
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    // Connect to WiFi network
     WiFi.enableAP(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    
    #if !USE_DHCP
    IPAddress ip,netmask,gateway,primaryDNS,secondaryDNS;
    ip.fromString(LOCAL_IP);
    netmask.fromString(NETMASK);
    gateway.fromString(GATEWAY);
    primaryDNS.fromString(PRIMARY_DNS);
    secondaryDNS.fromString(SECONDARY_DNS);
    if (!WiFi.config(ip, gateway, netmask, primaryDNS, secondaryDNS))
    {
      DEBUG_PRINTLN("Error: WiFi configuration failed");
      LOG_TO_FILE_E("Error: WiFi configuration failed");
    }
    #endif/*!USE_DHCP*/

    WiFi.begin(_ssid_.c_str(), _password_.c_str());
    DEBUG_PRINTLN("");

    unsigned long timeout = NTPTime::seconds() + (WIFI_CONNECTION_TIME_OUT * (gWiFiMode == WiFiMode::Initializing) ? 3 : 1);
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      DEBUG_PRINTLN(".");

      if (NTPTime::seconds() > timeout)
      {
        DEBUG_PRINTLN("Failed connecting to the network: timeout error!!!");
        LOG_TO_FILE_E("Start AccessPoint: failed to connect to %s.",_ssid_.c_str());
        WiFi.enableAP(true);
        StartAccessPointMode();
        return;
        //esp_restart();
      }
    }

    gWiFiMode = WiFiMode::Station;

    //Initialize the time getting it from the WEB We do it after we have WifFi connection
    NTPTime::initialize(SettingsMngr.wbsTimeZone.c_str());

    DEBUG_PRINTLN("--------------------");
    DEBUG_PRINTF("Connected to %s\n",_ssid_.c_str());
    DEBUG_PRINTF("IP address: %s\n",WiFi.localIP().toString().c_str());
    LOG_TO_FILE_I("Connected to %s",_ssid_.c_str());
  }
}
