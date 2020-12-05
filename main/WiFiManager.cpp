#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include "WiFiManager.h"
#include "DebugPrint.h"
extern "C" {
  #include <esp_wifi.h>
}

void WiFiConnect(const String &_ssid_, const String &_password_)
{
  esp_wifi_set_ps(WIFI_PS_NONE);
  if (WiFi.status() != WL_CONNECTED)
  {
    // Connect to WiFi network
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    WiFi.begin(_ssid_.c_str(), _password_.c_str());
    DEBUG_PRINTLN("");

    unsigned long timeout = millis() + WIFI_CONNECTION_TIME_OUT;
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      DEBUG_PRINT(".");

      if (millis() > timeout)
      {
        DEBUG_PRINTLN("Failed connecting to the network: timeout error!!!");
        esp_restart();
      }
    }

    DEBUG_PRINTLN("--------------------");
    DEBUG_PRINTF("Connected to %s\n",_ssid_.c_str());
    DEBUG_PRINTF("IP address: %s\n",WiFi.localIP().toString().c_str());
  }
}
