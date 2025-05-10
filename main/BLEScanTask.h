#ifndef BLESCAN_TASK_H
#define BLESCAN_TASK_H
#include "TrackedDeviceList.h"

namespace BLEScanTask
{
    void InitializeBLEScan();
    void BLEScanloop();
    void StartBLEScanTask();
    bool IsScanEnabled();
    void GetTrackedDevices(std::vector<BLETrackedDevice> &devices);
    void ForceBatteryRead(const char *normalizedAddr);
    void batteryTask();
    size_t GetTrackedDevicesCount();
    TrackedDeviceList& GetTrackedDeviceList();
}

#endif // BLESCAN_TASK_H