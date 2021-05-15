#include "bletasks.h"
#include <BLEDevice.h>
#include "utility.h"
#include "SPIFFSLogger.h"
#include "watchdog.h"
#include "NetworkSync.h"
namespace BLETask
{
    BLEScan *pBLEScan;
    static bool scanCompleted = false;

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

            CRITICALSECTION_WRITESTART(trackedDevicesMutex)
            for (auto &trackedDevice : BLETrackedDevices)
            {
                if (strcmp(address, trackedDevice.address) == 0)
                {
#if NUM_OF_ADVERTISEMENT_IN_SCAN > 1
                    trackedDevice.advertisementCounter++;
                    //To proceed we have to find at least NUM_OF_ADVERTISEMENT_IN_SCAN duplicates during the scan
                    //and the code have to be executed only once
                    if (trackedDevice.advertisementCounter != NUM_OF_ADVERTISEMENT_IN_SCAN)
                        return;
#endif

                    if (!trackedDevice.advertised) //Skip advertised dups
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
                                LOG_TO_FILE_D("Device %s within range, RSSI: %d ", address, RSSI);
                        }
                        else
                        {
                            DEBUG_PRINTF("INFO: Tracked device discovered, Address: %s , RSSI: %d\n", address, RSSI);
                        }
                    }
                    return;
                }
            }

            //This is a new device...
            BLETrackedDevice trackedDevice;
            trackedDevice.advertised = NUM_OF_ADVERTISEMENT_IN_SCAN <= 1; //Skip duplicates
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
            BLETrackedDevices.push_back(std::move(trackedDevice));
            FastDiscovery[trackedDevice.address] = true;
#if NUM_OF_ADVERTISEMENT_IN_SCAN > 1
            //To proceed we have to find at least NUM_OF_ADVERTISEMENT_IN_SCAN duplicates during the scan
            //and the code have to be executed only once
            return;
#endif
            CRITICALSECTION_WRITEEND;

            DEBUG_PRINTF("INFO: Device discovered, Address: %s , RSSI: %d\n", address, RSSI);
            if (advertisedDevice.haveName())
                LOG_TO_FILE_D("Discovered new device %s ( %s ) within range, RSSI: %d ", address, shortName, RSSI);
            else
                LOG_TO_FILE_D("Discovered new device %s within range, RSSI: %d ", address, RSSI);
        }
    };

    void Initialize()
    {
        BLEDevice::init(GATEWAY_NAME);
        pBLEScan = BLEDevice::getScan();
        pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), NUM_OF_ADVERTISEMENT_IN_SCAN > 1);
        pBLEScan->setActiveScan(ACTIVE_SCAN);
        pBLEScan->setInterval(50);
        pBLEScan->setWindow(50);
    }

    bool ScanCompleted()
    {
        return scanCompleted;
    }

    void Scan()
    {
#if PROGRESSIVE_SCAN
        static uint32_t elapsedScanTime = 0;
        static uint32_t lastScanTime = 0;

        bool continuePrevScan = elapsedScanTime > 0;
        if (!continuePrevScan) //new scan
        {
            //Reset the states of discovered devices
            for (auto &trackedDevice : BLETrackedDevices)
            {
                trackedDevice.advertised = false;
                trackedDevice.rssiValue = -100;
                trackedDevice.advertisementCounter = 0;
            }
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
#else
        //Reset the states of discovered devices
        for (auto &trackedDevice : BLETrackedDevices)
        {
            trackedDevice.advertised = false;
            trackedDevice.rssiValue = -100;
            trackedDevice.advertisementCounter = 0;
        }

        scanCompleted = false;
        //DEBUG_PRINTF("\n*** Memory Before scan: %u\n",xPortGetFreeHeapSize());
        NetworkAcquire();
        pBLEScan->start(SettingsMngr.scanPeriod);
        pBLEScan->stop();
        pBLEScan->clearResults();
        NetworkRelease();
        scanCompleted = true;
        //DEBUG_PRINTF("\n*** Memory After scan: %u\n",xPortGetFreeHeapSize());
#endif
    }

    static BLEUUID service_BATT_UUID(BLEUUID((uint16_t)0x180F));
    static BLEUUID char_BATT_UUID(BLEUUID((uint16_t)0x2A19));

#if PUBLISH_BATTERY_LEVEL
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

    void ForceBatteryRead(const char *normalizedAddr)
    {
        for (auto &trackedDevice : BLETrackedDevices)
        {
            if (strcmp(trackedDevice.address, normalizedAddr) == 0)
            {
                trackedDevice.forceBatteryRead = true;
                return;
            }
        }
    }

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

        NetworkAcquire();

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
            //Before disconnecting I need to pause the task to wait (I don't know what), otherwise we have an heap corruption
            //delay(200);
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
            //We fail to connect and we have to be sure the PeerDevice is removed before delete it
            BLEDevice::removePeerDevice(client.m_appId, true);
            log_i("-------------------Not connected!!!--------------------");
        }

        log_i("<< ------------------batteryLevel----------------- ");
        NetworkRelease();
        return bleconnected;
    }

    void ReadBatteryForDevices(void (*callback)(void), bool progressiveMode)
    {
        //DEBUG_PRINTF("\n*** Memory before battery scan: %u\n",xPortGetFreeHeapSize());
#if PROGRESSIVE_SCAN
        if (!scanCompleted)
            return;
#endif

        for (auto &trackedDevice : BLETrackedDevices)
        {
            if (!(SettingsMngr.InBatteryList(trackedDevice.address) || trackedDevice.forceBatteryRead))
                continue;

            if (callback != nullptr)
                callback();

            //We need to connect to the device to read the battery value
            //So that we check only the device really advertised by the scan
            unsigned long BatteryReadTimeout = trackedDevice.lastBattMeasureTime + BATTERY_READ_PERIOD;
            unsigned long BatteryRetryTimeout = trackedDevice.lastBattMeasureTime + BATTERY_RETRY_PERIOD;
            unsigned long now = NTPTime::getTimeStamp();
            bool batterySet = trackedDevice.batteryLevel > 0;
            bool reasonReadTimeoutElapsed = batterySet && (BatteryReadTimeout < now);
            bool reasonRetryTimeoutElapsed = !batterySet && (BatteryRetryTimeout < now);
            bool reasonForced = trackedDevice.forceBatteryRead;
            trackedDevice.forceBatteryRead = false; //reset flag
            if (trackedDevice.advertised && trackedDevice.hasBatteryService && trackedDevice.rssiValue > -90 &&
                (reasonReadTimeoutElapsed || reasonRetryTimeoutElapsed || reasonForced))
            {
                DEBUG_PRINTF("\nReading Battery level for %s: Retries: %d\n", trackedDevice.address, trackedDevice.connectionRetry);
                DEBUG_PRINTF("Reason : %s\n", reasonForced ? "Forced" : (reasonReadTimeoutElapsed ? "ReadTimeoutElapsed" : "RetryTimeoutElapsed"));
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
                        //Report the error only one time if Log level info is set
                        LOG_TO_FILE_E("Error: Connection to device %s failed", trackedDevice.address);
                        DEBUG_PRINTF("Error: Connection to device %s failed", trackedDevice.address);
                    }
                    else
                    {
                        //Report the error every time if Log level debug or verbose is set
                        LOG_TO_FILE_D("Error: Connection to device %s failed", trackedDevice.address);
                        DEBUG_PRINTF("Error: Connection to device %s failed", trackedDevice.address);
                    }
                }

                if (progressiveMode)
                    break; //We read only the battery for one device at every call
            }
            else if (BatteryReadTimeout < now)
            {
                //Here we preserve the lastBattMeasure time to trigger a new read
                //when the device will be advertised again
                trackedDevice.batteryLevel = -1;
            }

            Watchdog::Feed();
        }
        //DEBUG_PRINTF("\n*** Memory after battery scan: %u\n",xPortGetFreeHeapSize());
    }
#endif
}