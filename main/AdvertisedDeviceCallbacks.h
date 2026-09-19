#ifndef ADVERTISED_DEVICE_CALLBACKS_H
#define ADVERTISED_DEVICE_CALLBACKS_H
#include "DebugPrint.h"
#include "BleDeviceId.h"
#include "settings.h"
#include "watchdog.h"
#include "SPIFFSLogger.h"
#include <BLEDevice.h>
#include <BLEAdvertisedDevice.h>
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{

  void onResult(BLEAdvertisedDevice advertisedDevice) override
  {
    Watchdog::Feed();
    const uint8_t shortNameSize = 31;
    char deviceIdAsString[BleDeviceId::UUID_STRING_SIZE];
    char shortName[shortNameSize];
    memset(shortName, 0, shortNameSize);
    memset(deviceIdAsString, 0, BleDeviceId::UUID_STRING_SIZE);
    BleDeviceId deviceId;
    bool isIBeacon = false;
    // Check for iBeacon manufacturer data
    if (advertisedDevice.haveManufacturerData())
    {
        DEBUG_PRINT("INFO: Device discovered, has manufacturer data\n");
        std::string manuData = advertisedDevice.getManufacturerData();
    
        // iBeacon format: 
        // Length: 25 bytes
        // Company ID: 0x004C (Apple, little endian)
        // Type: 0x02 0x15 (iBeacon)
        // UUID: 16 bytes
        // Major: 2 bytes
        // Minor: 2 bytes
        // RSSI at 1m: 1 byte
            
        if (manuData.length() >= 25) 
        {
            uint8_t* data = (uint8_t*)manuData.data();
            
            // Check Apple company ID (little endian)
            if (data[0] == 0x4C && data[1] == 0x00 && data[2] == 0x02 && data[3] == 0x15) 
            {
                isIBeacon = true;
                // Valid iBeacon detected
                // Extract UUID (bytes 4-19)
                // Extract major (bytes 20-21) 
                // Extract minor (bytes 22-23)
                uint8_t uuid[16];
                memcpy(uuid, data + 4, 16);
            
                uint16_t major = (data[20] << 8) | data[21];
                uint16_t minor = (data[22] << 8) | data[23];
                deviceId = BleDeviceId(uuid, true, major, minor);
                deviceId.toString(deviceIdAsString);
                shortName[0] = '\0';
                if (advertisedDevice.haveName())
                {
                strncpy(shortName, advertisedDevice.getName().c_str(), shortNameSize - 1);
                }

                DEBUG_PRINTF("INFO: Device discovered is iBeacon: %s (%s)\n", deviceIdAsString, shortName);
            }
        }
    }

    if(!isIBeacon)
    {
        deviceId = BleDeviceId((const uint8_t *)advertisedDevice.getAddress().getNative(), false);
        deviceId.toString(deviceIdAsString);
        shortName[0] = '\0';
        if (advertisedDevice.haveName())
        {
          strncpy(shortName, advertisedDevice.getName().c_str(), shortNameSize - 1);
        }

        DEBUG_PRINTF("INFO: Device discovered is not iBeacon: %s (%s)\n", deviceIdAsString, shortName);
    }

    if (!SettingsMngr.IsTraceable(deviceId))
      return;


    int RSSI = advertisedDevice.getRSSI();

    deviceId.toString(deviceIdAsString);

    CRITICALSECTION_WRITESTART(trackedDevicesMutex)
    for (auto &trackedDevice : BLETrackedDevices)
    {
      if (deviceId == trackedDevice.deviceId)
      {
#if NUM_OF_ADVERTISEMENT_IN_SCAN > 1
        trackedDevice.advertisementCounter++;
        // To proceed we have to find at least NUM_OF_ADVERTISEMENT_IN_SCAN duplicates during the scan
        // and the code have to be executed only once
        if (trackedDevice.advertisementCounter != NUM_OF_ADVERTISEMENT_IN_SCAN)
          return;
#endif

        if (!trackedDevice.advertised) // Skip advertised dups
        {
          trackedDevice.addressType = advertisedDevice.getAddressType();
          trackedDevice.advertised = true;
          trackedDevice.lastDiscoveryTime = NTPTime::seconds();
          trackedDevice.rssiValue = RSSI;
          if (!trackedDevice.isDiscovered)
          {
            trackedDevice.isDiscovered = true;
            trackedDevice.connectionRetry = 0;
            FastDiscovery[trackedDevice.deviceId] = true;
            DEBUG_PRINTF("INFO: Tracked device discovered again, Address: %s , RSSI: %d\n", deviceIdAsString, RSSI);
            if (advertisedDevice.haveName())
            {
              LOG_TO_FILE_D("Device %s ( %s ) within range, RSSI: %d ", deviceIdAsString, shortName, RSSI);
            }
            else
              LOG_TO_FILE_D("Device %s within range, RSSI: %d ", deviceIdAsString, RSSI);
          }
          else
          {
            DEBUG_PRINTF("INFO: Tracked device discovered, Address: %s , RSSI: %d\n", deviceIdAsString, RSSI);
          }
        }
        return;
      }
    }

    // This is a new device...
    BLETrackedDevice trackedDevice;
    trackedDevice.advertised = NUM_OF_ADVERTISEMENT_IN_SCAN <= 1; // Skip duplicates
    trackedDevice.deviceId = deviceId;
    trackedDevice.addressType = advertisedDevice.getAddressType();
    trackedDevice.isDiscovered = NUM_OF_ADVERTISEMENT_IN_SCAN <= 1;
    trackedDevice.lastDiscoveryTime = NTPTime::seconds();
    trackedDevice.lastBattMeasureTime = 0;
    trackedDevice.batteryLevel = -1;
    trackedDevice.hasBatteryService = true;
    trackedDevice.connectionRetry = 0;
    trackedDevice.rssiValue = RSSI;
    trackedDevice.advertisementCounter = 1;
    BLETrackedDevices.push_back(std::move(trackedDevice));
    FastDiscovery[trackedDevice.deviceId] = true;
#if NUM_OF_ADVERTISEMENT_IN_SCAN > 1
    // To proceed we have to find at least NUM_OF_ADVERTISEMENT_IN_SCAN duplicates during the scan
    // and the code have to be executed only once
    return;
#endif
    CRITICALSECTION_WRITEEND;

    DEBUG_PRINTF("INFO: Device discovered, Address: %s , RSSI: %d\n", deviceIdAsString, RSSI);
    if (advertisedDevice.haveName())
      LOG_TO_FILE_D("Discovered new device %s ( %s ) within range, RSSI: %d ", deviceIdAsString, shortName, RSSI);
    else
      LOG_TO_FILE_D("Discovered new device %s within range, RSSI: %d ", deviceIdAsString, RSSI);
  }
};
#endif // ADVERTISED_DEVICE_CALLBACKS_H