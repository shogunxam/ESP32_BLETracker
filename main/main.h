#ifndef MAIN_H
#define MAIN_H
#include <BLEDevice.h>
#include <WString.h>
#include <sstream>
#include <iomanip>
#include "firmwarever.h"

#define ADDRESS_STRING_SIZE 13
#define RSSI_STRING_SIZE 5
#define BATTERY_STRING_SIZE 5

struct BLETrackedDevice
{
  char address[ADDRESS_STRING_SIZE];
  bool isDiscovered;  //Until it's TRUE the device is considered Online, if it's not dcosvered for a period it become FALSE
  long lastDiscoveryTime;
  long lastBattMeasureTime;
  int8_t batteryLevel;
  bool advertised;  //TRUE if the device is just advertised in the current scan
  bool hasBatteryService;//Used to avoid connections with BLE without battery service
  uint8_t connectionRetry;//Number of retries if the connection with the device fails
  int8_t rssiValue; 

  esp_ble_addr_type_t addressType;
  BLETrackedDevice()
  {
    address[0] ='\0';
    isDiscovered = 0;
    lastDiscoveryTime = 0;
    lastBattMeasureTime = 0;
    batteryLevel = -1;
    advertised = false;
    hasBatteryService = false;
    connectionRetry = 0;
    rssiValue = -100;
    addressType = BLE_ADDR_TYPE_PUBLIC;
  }
} ;

char* formatMillis(unsigned long milliseconds, char outStr[20]);

#endif /*MAIN_H*/
