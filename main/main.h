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
  char rssi[RSSI_STRING_SIZE];
  bool isDiscovered;
  long lastDiscovery;
  long lastBattMeasure;
  char batteryLevel[BATTERY_STRING_SIZE];
  bool advertised;  //TRUE if the device is just advertised
  bool hasBatteryService;//Used to avoid connections with BLE without battery service
  int connectionRetry;//Number of retries if the connection with the device fails
  esp_ble_addr_type_t addressType;
  BLETrackedDevice()
  {
    address[0] ='\0';
    rssi[0]='\0';
    isDiscovered = 0;
    lastDiscovery = 0;
    lastBattMeasure = 0;
    batteryLevel[0] = '\0';
    advertised = false;
    hasBatteryService = false;
    connectionRetry = 0;
    addressType = BLE_ADDR_TYPE_PUBLIC;
  }
} ;

String formatMillis(unsigned long milliseconds);

#endif /*MAIN_H*/
