#include "BLEScanTask.h"
#include "main.h"
#include "settings.h"
#include "DebugPrint.h"
#include "BLEScan.h"
#include "SPIFFSLogger.h"
#include "NTPTime.h"
#include "watchdog.h"
#include "utility.h"
#include "WiFiManager.h"
#include "TrackedDeviceList.h"

namespace BLEScanTask
{
    static std::map<std::string, bool> FastDiscovery;
    static TrackedDeviceList BLETrackedDevices;

    class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
    {
        void onResult(BLEAdvertisedDevice advertisedDevice) override
        {
            Watchdog::Feed();
            const uint8_t shortNameSize = 31;
            char address[ADDRESS_STRING_SIZE];
            NormalizeAddress(*(advertisedDevice.getAddress().getNative()), address);

            if (!SettingsMngr.IsTraceable(address))
                return;

            char shortName[shortNameSize];
            memset(shortName, 0, shortNameSize);
            if (advertisedDevice.haveName())
                strncpy(shortName, advertisedDevice.getName().c_str(), shortNameSize - 1);

            int RSSI = advertisedDevice.getRSSI();

            bool done = BLETrackedDevices.ExcuteFunctionOnDevice(address, [&](BLETrackedDevice &trackedDevice)
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
                            FastDiscovery[trackedDevice.address] = true;
                            DEBUG_PRINTF("INFO: Tracked device discovered again, Address: %s , RSSI: %d\n", address, RSSI);
                            if (advertisedDevice.haveName())
                            {
                                LOG_TO_FILE_D("Device %s ( %s ) within range, RSSI: %d ", address, shortName, RSSI);
                            }
                            else
                            {
                                LOG_TO_FILE_D("Device %s within range, RSSI: %d ", address, RSSI);
                            }
                        }
                        else
                        {
                            DEBUG_PRINTF("INFO: Tracked device discovered, Address: %s , RSSI: %d\n", address, RSSI);
                        }
                    } });

            if (done)
            {
                return;
            }

            // This is a new device...
            BLETrackedDevice trackedDevice;
            trackedDevice.advertised = NUM_OF_ADVERTISEMENT_IN_SCAN <= 1; // Skip duplicates
            memcpy(trackedDevice.address, address, ADDRESS_STRING_SIZE);
            trackedDevice.addressType = advertisedDevice.getAddressType();
            trackedDevice.isDiscovered = NUM_OF_ADVERTISEMENT_IN_SCAN <= 1;
            trackedDevice.lastDiscoveryTime = NTPTime::seconds();
            trackedDevice.lastBattMeasureTime = 0;
            trackedDevice.batteryLevel = -1;
            trackedDevice.hasBatteryService = true;
            trackedDevice.connectionRetry = 0;
            trackedDevice.rssiValue = RSSI;
            trackedDevice.advertisementCounter = 1;
            BLETrackedDevices.AddDevice(trackedDevice);
            FastDiscovery[trackedDevice.address] = true;
#if NUM_OF_ADVERTISEMENT_IN_SCAN > 1
            // To proceed we have to find at least NUM_OF_ADVERTISEMENT_IN_SCAN duplicates during the scan
            // and the code have to be executed only once
            return;
#endif

            DEBUG_PRINTF("INFO: Device discovered, Address: %s , RSSI: %d\n", address, RSSI);
            if (advertisedDevice.haveName())
                LOG_TO_FILE_D("Discovered new device %s ( %s ) within range, RSSI: %d ", address, shortName, RSSI);
            else
                LOG_TO_FILE_D("Discovered new device %s within range, RSSI: %d ", address, RSSI);
        }
    };

    BLEScan *pBLEScan = nullptr;

    void GetTrackedDevices(std::vector<BLETrackedDevice> &devices)
    {
        BLETrackedDevices.GetDevices(devices);
    }

    void ResetDeviceState()
    {
        log_e("Resetting device state...");
        BLETrackedDevices.ExcuteFunctionOnAllDevices([](BLETrackedDevice &trackedDevice)
                                                     {
            trackedDevice.advertised = false;
            trackedDevice.rssiValue = -100;
            trackedDevice.advertisementCounter = 0; });
    }

    void InitializeBLEScan()
    {
        BLETrackedDevices.Initialize();
        if (pBLEScan == nullptr)
        {
            BLEDevice::init(SettingsMngr.gateway.c_str());
            pBLEScan = BLEDevice::getScan();
            pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), NUM_OF_ADVERTISEMENT_IN_SCAN > 1);
            pBLEScan->setActiveScan(ACTIVE_SCAN);
            pBLEScan->setInterval(50);
            pBLEScan->setWindow(50);
        }
    }

    void ForceBatteryRead(const char *normalizedAddr)
    {
        BLETrackedDevices.ExcuteFunctionOnDevice(normalizedAddr, [&](BLETrackedDevice &trackedDevice)
                                                 { trackedDevice.forceBatteryRead = true; });
    }

    bool IsScanEnabled()
    {
        log_e("IsmanualScanOn: %s\n",  SettingsMngr.IsManualScanOn() ? "true" : "false");
        log_e("IsManualScanEnabled: %s\n",  SettingsMngr.IsManualScanEnabled() ? "true" : "false");
        return SettingsMngr.IsManualScanOn() || !SettingsMngr.IsManualScanEnabled();
    }

    size_t GetTrackedDevicesCount()
    {
        return BLETrackedDevices.Size();
    }

    TrackedDeviceList& GetTrackedDeviceList() { return BLETrackedDevices; }

#if PUBLISH_BATTERY_LEVEL
    static BLEUUID service_BATT_UUID(BLEUUID((uint16_t)0x180F));
    static BLEUUID char_BATT_UUID(BLEUUID((uint16_t)0x2A19));

    class MyBLEClientCallBack : public BLEClientCallbacks
    {
        void onConnect(BLEClient *pClient)
        {
        }

        virtual void onDisconnect(BLEClient *pClient)
        {
            log_i(" >> onDisconnect callback");
            pClient->disconnect();
        }
    };

    bool batteryLevel(const char address[ADDRESS_STRING_SIZE], esp_ble_addr_type_t addressType, int8_t &battLevel, bool &hasBatteryService)
    {
        log_i(">> ------------------batteryLevel----------------- ");
        bool bleconnected;
        BLEClient client;
        battLevel = -1;
        static char canonicalAddress[ADDRESS_STRING_SIZE + 5];
        CanonicalAddress(address, canonicalAddress);
        BLEAddress bleAddress = BLEAddress(canonicalAddress);
        log_i("connecting to : %s", bleAddress.toString().c_str());
        LOG_TO_FILE_D("Reading battery level for device %s", address);
        MyBLEClientCallBack callback;
        client.setClientCallbacks(&callback);

        // Connect to the remote BLE Server.
        bleconnected = client.connect(bleAddress, addressType);
        if (bleconnected)
        {
            log_i("Connected to server");
            BLERemoteService *pRemote_BATT_Service = client.getService(service_BATT_UUID);
            if (pRemote_BATT_Service == nullptr)
            {
                log_i("Cannot find the BATTERY service.");
                LOG_TO_FILE_E("Error: Cannot find the BATTERY service for the device %s", address);
                hasBatteryService = false;
            }
            else
            {
                BLERemoteCharacteristic *pRemote_BATT_Characteristic = pRemote_BATT_Service->getCharacteristic(char_BATT_UUID);
                if (pRemote_BATT_Characteristic == nullptr)
                {
                    log_i("Cannot find the BATTERY characteristic.");
                    LOG_TO_FILE_E("Error: Cannot find the BATTERY characteristic for device %s", address);
                    hasBatteryService = false;
                }
                else
                {
                    std::string value = pRemote_BATT_Characteristic->readValue();
                    if (value.length() > 0)
                        battLevel = (int8_t)value[0];
                    log_i("Reading BATTERY level : %d", battLevel);
                    LOG_TO_FILE_I("Battery level for device %s is %d", address, battLevel);
                    hasBatteryService = true;
                }
            }
            // Before disconnecting I need to pause the task to wait (I don't know what), otherwise we have an heap corruption
            // delay(200);
            if (client.isConnected())
            {
                log_i("disconnecting...");
                client.disconnect();
            }
            log_i("waiting for disconnection...");
            while (client.isConnected())
                delay(100);
            log_i("Client disconnected.");
        }
        else
        {
            // We fail to connect and we have to be sure the PeerDevice is removed before delete it
            BLEDevice::removePeerDevice(client.m_appId, true);
            log_i("-------------------Not connected!!!--------------------");
        }

        log_i("<< ------------------batteryLevel----------------- ");
        return bleconnected;
    }

    void batteryTask()
    {
        // DEBUG_PRINTF("\n*** Memory before battery scan: %u\n",xPortGetFreeHeapSize());
        log_e("batteryTask...");     
        BLETrackedDevices.ExcuteFunctionOnAllDevices([](BLETrackedDevice &trackedDevice)
                                                     {
            if (!(SettingsMngr.InBatteryList(trackedDevice.address) || trackedDevice.forceBatteryRead))
                return;
                // We need to connect to the device to read the battery value
            // So that we check only the device really advertised by the scan
            unsigned long BatteryReadTimeout = trackedDevice.lastBattMeasureTime + BATTERY_READ_PERIOD;
            unsigned long BatteryRetryTimeout = trackedDevice.lastBattMeasureTime + BATTERY_RETRY_PERIOD;
            unsigned long now = NTPTime::getTimeStamp();
            bool batterySet = trackedDevice.batteryLevel > 0;
            if (trackedDevice.advertised && trackedDevice.hasBatteryService && trackedDevice.rssiValue > -90 &&
                ((batterySet && (BatteryReadTimeout < now)) ||
                (!batterySet && (BatteryRetryTimeout < now)) ||
                trackedDevice.forceBatteryRead))
            {
                DEBUG_PRINTF("\nReading Battery level for %s: Retries: %d\n", trackedDevice.address, trackedDevice.connectionRetry);
                bool connectionEstablished = batteryLevel(trackedDevice.address, trackedDevice.addressType, trackedDevice.batteryLevel, trackedDevice.hasBatteryService);
                if (connectionEstablished || !trackedDevice.hasBatteryService)
                {
                    log_i("Device %s has battery service: %s", trackedDevice.address, trackedDevice.hasBatteryService ? "YES" : "NO");
                    trackedDevice.connectionRetry = 0;
                    trackedDevice.lastBattMeasureTime = now;
                }
                else
                {
                    trackedDevice.connectionRetry++;
                    if (trackedDevice.connectionRetry >= MAX_BLE_CONNECTION_RETRIES)
                    {
                        trackedDevice.connectionRetry = 0;
                        trackedDevice.lastBattMeasureTime = now;
                        // Report the error only one time if Log level info is set
                        LOG_TO_FILE_E("Error: Connection to device %s failed", trackedDevice.address);
                        DEBUG_PRINTF("Error: Connection to device %s failed", trackedDevice.address);
                    }
                    else
                    {
                        // Report the error every time if Log level debug or verbose is set
                        LOG_TO_FILE_D("Error: Connection to device %s failed", trackedDevice.address);
                        DEBUG_PRINTF("Error: Connection to device %s failed", trackedDevice.address);
                    }
                }
            }
            else if (BatteryReadTimeout < now)
            {
                // Here we preserve the lastBattMeasure time to trigger a new read
                // when the device will be advertised again
                trackedDevice.batteryLevel = -1;
            }
            trackedDevice.forceBatteryRead = false;
            Watchdog::Feed(); });
    }
    // DEBUG_PRINTF("\n*** Memory after battery scan: %u\n",xPortGetFreeHeapSize());
#else
    void batteryTask() {}
#endif

    bool PerformBleScan()
    {
#if PROGRESSIVE_SCAN
        bool scanCompleted = false;
#endif
        log_e("PerformBleScan start");
        if (IsScanEnabled())
        {
#if PROGRESSIVE_SCAN
            static uint32_t elapsedScanTime = 0;
            static uint32_t lastScanTime = 0;

            bool continuePrevScan = elapsedScanTime > 0;
            if (!continuePrevScan) // new scan
            {
                // Reset the states of discovered devices
                ResetDeviceState();
            }

            lastScanTime = NTPTime::seconds();
            pBLEScan->start(1, continuePrevScan);
            pBLEScan->stop();
            elapsedScanTime += NTPTime::seconds() - lastScanTime;
            scanCompleted = elapsedScanTime > SettingsMngr.scanPeriod;
            if (scanCompleted)
            {
                elapsedScanTime = 0;
                pBLEScan->clearResults();
            }
#else // !PROGRESSIVE_SCAN
      // Reset the states of discovered devices
            log_e("PerformBleScan...");
            ResetDeviceState();

            //DEBUG_PRINTF("\n*** Memory Before scan: %u\n", xPortGetFreeHeapSize());
            pBLEScan->start(SettingsMngr.scanPeriod);
            pBLEScan->stop();
            pBLEScan->clearResults();
            //DEBUG_PRINTF("\n*** Memory After scan: %u\n", xPortGetFreeHeapSize());
#endif
        }
        else
        {
            log_e("Scan disabled");
            ResetDeviceState();
            pBLEScan->stop();
            pBLEScan->clearResults();
        }
    }

    void ProcessBLEScanResults()
    {
        log_e("ProcessBLEScanResults...");
        BLETrackedDevices.ExcuteFunctionOnAllDevices([&](BLETrackedDevice &trackedDevice)
                                                     {

            if (trackedDevice.isDiscovered && (trackedDevice.lastDiscoveryTime + SettingsMngr.maxNotAdvPeriod) < NTPTime::seconds())
            {
                trackedDevice.isDiscovered = false;
                FastDiscovery[trackedDevice.address] = false;
                LOG_TO_FILE_D("Devices %s is gone out of range", trackedDevice.address);
            } });
    }

    void BLEScanloop()
    {
        DEBUG_PRINTLN("BLEScanTask loop");
        PerformBleScan();
        ProcessBLEScanResults();
    }

    void BLEScanTask(void *param)
    {
        while (true)
        {
            DEBUG_PRINTLN("BLEScanTask");
            if (!WiFiManager::IsAccessPointModeOn())
            {
                BLEScanloop();
            }
            Watchdog::Feed();
            delay(100);
        }
    }

    void StartBLEScanTask()
    {
        InitializeBLEScan();
        xTaskCreatePinnedToCore(
            BLEScanTask,   /* Function to implement the task */
            "BLEScanTask", /* Name of the task */
            4096,          /* Stack size in words */
            NULL,          /* Task input parameter */
            20,            /* Priority of the task */
            NULL,          /* Task handle. */
            1);
    }
}