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
  #if TRACK_BEACONS
  bool GetAppleIBeaconDeviceID(uint8_t *data, uint8_t manuDataLength, BleDeviceId &deviceId)
  {
    if (manuDataLength < 25)
    {
      return false;
    }

    if (!(data[0] == 0x4C && data[1] == 0x00 && data[2] == 0x02 && data[3] == 0x15))
    {
      return false;
    }

    uint8_t uuid[16];
    memcpy(uuid, data + 4, 16);
    uint16_t major = (data[20] << 8) | data[21];
    uint16_t minor = (data[22] << 8) | data[23];
    deviceId = BleDeviceId(uuid, true, major, minor);
    return true;
  }

  bool GetAltBeaconDeviceID(uint8_t *data, uint8_t manuDataLength, BleDeviceId &deviceId)
  {
    if (manuDataLength < 26)
    {
      return false;
    }
    if (!(data[2] == 0xBE && data[3] == 0xAC))
    {
      return false;
    }
    uint8_t beaconId[20];
    memcpy(beaconId, data + 4, 16);
    uint16_t major = (data[20] << 8) | data[21];
    uint16_t minor = (data[22] << 8) | data[23];
    deviceId = BleDeviceId(beaconId, true, major, minor);
    return true;
  }

  bool GetIBeaconId(BLEAdvertisedDevice &advertisedDevice, BleDeviceId &deviceId)
  {
    if (advertisedDevice.haveManufacturerData())
    {
      std::string manuData = advertisedDevice.getManufacturerData();
      uint8_t *data = (uint8_t *)manuData.data();
      uint8_t manuDataLength = manuData.length();

      return GetAppleIBeaconDeviceID(data, manuDataLength, deviceId) ||
             GetAltBeaconDeviceID(data, manuDataLength, deviceId);
    }
    return false;
  }

  bool GetEddyStoneBeaconId(BLEAdvertisedDevice &advertisedDevice, BleDeviceId &deviceId)
  {
    if (!advertisedDevice.haveServiceData())
    {
      return false;
    }

    static const BLEUUID eddystoneUUID("0000feaa-0000-1000-8000-00805f9b34fb");
    int numServiceData = advertisedDevice.getServiceDataCount();
    for (int i = 0; i < numServiceData; i++)
    {
      BLEUUID serviceDataUUID = advertisedDevice.getServiceDataUUID(i);
      if (serviceDataUUID.equals(eddystoneUUID))
      {
        std::string serviceData = advertisedDevice.getServiceData(i);
        uint8_t *data = (uint8_t *)serviceData.data();
        size_t len = serviceData.length();
        if (len>=18 && data[0] == 0x00)
        {
          uint8_t beaconId[16];
          memcpy(beaconId, data + 2, 16);
          deviceId = BleDeviceId(beaconId, true, 0, 0);
          return true;
        }
      }
    }
    return false;
  }

  bool GetDeviceIdFromBeacon(BLEAdvertisedDevice &advertisedDevice, BleDeviceId &deviceId)
  {
    return (GetIBeaconId(advertisedDevice, deviceId) || GetEddyStoneBeaconId(advertisedDevice, deviceId));
  }
  #endif

  void onResult(BLEAdvertisedDevice advertisedDevice) override
  {
    Watchdog::Feed();
    const uint8_t shortNameSize = 31;
    char deviceIdAsString[BleDeviceId::UUID_STRING_SIZE];
    char shortName[shortNameSize];
    memset(shortName, 0, shortNameSize);
    memset(deviceIdAsString, 0, BleDeviceId::UUID_STRING_SIZE);
    BleDeviceId deviceId;
    #if TRACK_BEACONS
    bool isBeacon = GetDeviceIdFromBeacon(advertisedDevice, deviceId);
    
    if (!isBeacon)
    {
      deviceId = BleDeviceId((const uint8_t *)advertisedDevice.getAddress().getNative(), false);     
    }
    #else
    deviceId = BleDeviceId((const uint8_t *)advertisedDevice.getAddress().getNative(), false);
    bool isBeacon = false;
    #endif

    if (advertisedDevice.haveName())
    {
      strncpy(shortName, advertisedDevice.getName().c_str(), shortNameSize - 1);
    } 

    deviceId.toString(deviceIdAsString);
    DEBUG_PRINTF("INFO: Device discovered is%s a iBeacon: %s (%s)\n", isBeacon ? "" : " not", deviceIdAsString, shortName);

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