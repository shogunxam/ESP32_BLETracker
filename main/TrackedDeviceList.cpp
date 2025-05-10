#include "TrackedDeviceList.h"
#include "settings.h"

TrackedDeviceList::TrackedDeviceList():
    m_trackedDevicesMutex("TrackedDeviceList_Mutex")
{
}

void TrackedDeviceList::Initialize()
{
    m_trackedDevicesMutex.writeLock();
    m_trackedDevices.reserve(SettingsMngr.GetMaxNumOfTraceableDevices());
    for (const auto &dev : SettingsMngr.GetKnownDevicesList())
    {
        BLETrackedDevice trackedDevice;
        memcpy(trackedDevice.address, dev.address, ADDRESS_STRING_SIZE);
        trackedDevice.forceBatteryRead = dev.readBattery;
        m_trackedDevices.push_back(std::move(trackedDevice));
    }
    m_trackedDevicesMutex.writeUnlock();
}

void TrackedDeviceList::AddDevice(const BLETrackedDevice &device)
{
    m_trackedDevicesMutex.writeLock();
    m_trackedDevices.push_back(device);
    m_trackedDevicesMutex.writeUnlock();
}

void TrackedDeviceList::InsertOrUpdateDevice(const BLETrackedDevice &device)
{
    m_trackedDevicesMutex.writeLock();
    for (auto &trackedDevice : m_trackedDevices)
    {
        if (strcmp(trackedDevice.address, device.address) == 0)
        {
            trackedDevice = device;
            m_trackedDevicesMutex.writeUnlock();
            return;
        }
    }
    m_trackedDevices.push_back(device);
    m_trackedDevicesMutex.writeUnlock();
}

void TrackedDeviceList::Clear()
{
    m_trackedDevicesMutex.writeLock();
    m_trackedDevices.clear();
    m_trackedDevicesMutex.writeUnlock();
}

void TrackedDeviceList::ExcuteFunctionOnAllDevices(std::function<void(BLETrackedDevice &)> func)
{
    m_trackedDevicesMutex.readLock();
    for (auto &trackedDevice : m_trackedDevices)
    {
        func(trackedDevice);
    }
    m_trackedDevicesMutex.readUnlock();
}

bool TrackedDeviceList::ExcuteFunctionOnDevice(const char *address, std::function<void(BLETrackedDevice &)> func)
{
    bool done = false;
    m_trackedDevicesMutex.readLock();
    for (auto &trackedDevice : m_trackedDevices)
    {
        if (strcmp(trackedDevice.address, address) == 0)
        {
            func(trackedDevice);
            done = true;
            break;
        }
    }
    m_trackedDevicesMutex.readUnlock();
    return done;
}

void TrackedDeviceList::GetDevices(std::vector<BLETrackedDevice> &devices)
{
    m_trackedDevicesMutex.readLock();
    devices = m_trackedDevices;
    m_trackedDevicesMutex.readUnlock();
}

size_t TrackedDeviceList::Size() const
{
    m_trackedDevicesMutex.readLock();
    return m_trackedDevices.size();
     m_trackedDevicesMutex.readUnlock();
}
