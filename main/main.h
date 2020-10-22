#ifndef MAIN_H
#define MAIN_H
#include <BLEDevice.h>
#include <WString.h>
#include <sstream>
#include <iomanip>

#define VERSION "1.7R"

typedef struct
{
  String address;
  char rssi[4];
  bool isDiscovered;
  long lastDiscovery;
  long lastBattMeasure;
  int batteryLevel;
  bool advertised;  //TRUE if the device is just advertised
  bool hasBatteryService;//Used to avoid coonections with BLE without battery service
  int connectionRetry;//Number of retries if tje connection with the device fails
  esp_ble_addr_type_t addressType;
} BLETrackedDevice;

std::string formatMillis(unsigned long milliseconds);

#endif /*MAIN_H*/