#ifndef MAIN_H
#define MAIN_H
#include <BLEDevice.h>
#include <WString.h>
#include <sstream>
#include <iomanip>
#include "firmwarever.h"

typedef struct
{
  String address;
  String rssi;
  bool isDiscovered;
  long lastDiscovery;
  long lastBattMeasure;
  int batteryLevel;
  bool advertised;  //TRUE if the device is just advertised
  bool hasBatteryService;//Used to avoid connections with BLE without battery service
  int connectionRetry;//Number of retries if the connection with the device fails
  esp_ble_addr_type_t addressType;
} BLETrackedDevice;

String formatMillis(unsigned long milliseconds);

#endif /*MAIN_H*/
