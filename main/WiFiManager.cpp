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

WiFiClient wifiClient;

void WiFiConnect(const String &_ssid_, const String &_password_)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    // Connect to WiFi network
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

    unsigned long timeout = NTPTime::seconds() + WIFI_CONNECTION_TIME_OUT;
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      DEBUG_PRINT(".");

      if (NTPTime::seconds() > timeout)
      {
        DEBUG_PRINTLN("Failed connecting to the network: timeout error!!!");
        LOG_TO_FILE_E("Restart because failing to connect to %s.",_ssid_.c_str());
        esp_restart();
      }
    }

    //Initialize the time getting it from the WEB We do it after we have WifFi connection
    NTPTime::initialize();

    DEBUG_PRINTLN("--------------------");
    DEBUG_PRINTF("Connected to %s\n",_ssid_.c_str());
    DEBUG_PRINTF("IP address: %s\n",WiFi.localIP().toString().c_str());
    LOG_TO_FILE_I("Connected to %s",_ssid_.c_str());
  }
}
