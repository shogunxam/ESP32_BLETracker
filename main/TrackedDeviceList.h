#ifndef TRACKEDDEVICELIST_H
#define TRACKEDDEVICELIST_H
#include "constants.h"
#include <stdint.h>
#include <vector>
#include <functional>
#include <esp_gap_ble_api.h>

#include "myRWMutex.h"

struct BLETrackedDevice
{
    char address[ADDRESS_STRING_SIZE];
    bool isDiscovered; // Until it's TRUE the device is considered Online, if it's not discovered for a period it become FALSE
    long lastDiscoveryTime;
    long lastBattMeasureTime;
    int8_t batteryLevel;
    bool advertised;         // TRUE if the device is just advertised in the current scan
    bool hasBatteryService;  // Used to avoid connections with BLE without battery service
    uint8_t connectionRetry; // Number of retries if the connection with the device fails
    int8_t rssiValue;
    esp_ble_addr_type_t addressType;
    uint8_t advertisementCounter;
    bool forceBatteryRead;
    bool haDiscoveryPublished;

    BLETrackedDevice()
    {
        address[0] = '\0';
        isDiscovered = 0;
        lastDiscoveryTime = 0;
        lastBattMeasureTime = 0;
        batteryLevel = -1;
        advertised = false;
        hasBatteryService = true;
        connectionRetry = 0;
        rssiValue = -100;
        addressType = BLE_ADDR_TYPE_PUBLIC;
        advertisementCounter = 0;
        forceBatteryRead = true;
        haDiscoveryPublished = false;
    }
};

class TrackedDeviceList
{
public:
    TrackedDeviceList();
    ~TrackedDeviceList() = default;

    void Initialize();
    void AddDevice(const BLETrackedDevice &device);
    void InsertOrUpdateDevice(const BLETrackedDevice &device);
    void Clear();
    void ExcuteFunctionOnAllDevices(std::function<void(BLETrackedDevice &)>);
    bool ExcuteFunctionOnDevice(const char *address, std::function<void(BLETrackedDevice &)>);
    void GetDevices(std::vector<BLETrackedDevice> &devices);
    size_t Size() const;

private:
    std::vector<BLETrackedDevice> m_trackedDevices;
    mutable MyRWMutex m_trackedDevicesMutex;
};
#endif /// TRACKEDDEVICELIST_H