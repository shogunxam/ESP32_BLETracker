#ifndef __SETTINGS_H__
#define __SETTINGS_H__
#include <SPIFFS.h>
#include <WString.h>
#include <vector>
#include "main.h"

class Settings
{
public:
    struct KnownDevice{
        KnownDevice(const KnownDevice& dev);
        KnownDevice(const char* mac, bool batt, const char* desc);
        KnownDevice();
        KnownDevice& operator=(const KnownDevice& dev);
        char address[ADDRESS_STRING_SIZE];
        bool readBattery;
        char description[DESCRIPTION_STRING_SIZE];
    };

    Settings(const String &fileName = "", bool emptyLists = false);
    void SettingsFile(const String &fileName) { settingsFile = fileName; };
    String toJSON();
    static void StringListToArray(const String &whiteList, std::vector<String> &vStr);
    bool Save();
    void Load();
    void FactoryReset(bool emptyLists = false);
    String GetSettingsFile() { return settingsFile; }
    bool IsTraceable(const String &value);
    bool InBatteryList(const String &value);
    std::size_t GetMaxNumOfTraceableDevices();
    void EnableWhiteList(bool enable);
    void AddDeviceToList(const KnownDevice& device);
    void AddDeviceToList(const char mac[ADDRESS_STRING_SIZE], bool checkBattery, const char description[DESCRIPTION_STRING_SIZE]={'\0'});
    const std::vector<KnownDevice>& GetKnownDevicesList();
    String mqttUser;
    String mqttPwd;
    String mqttServer;
    uint16_t mqttPort;
    uint32_t scanPeriod;
    uint32_t maxNotAdvPeriod;
    uint8_t logLevel;
    bool mqttEnabled;

private:
    enum class DeviceProperty
    {
        eTraceable,
        eReadBattery
    };

    bool IsPropertyForDeviceEnabled(const String &value, DeviceProperty property);
    String ArrayToStringList(const std::vector<String> &whiteList);
    void SaveString(File file, const String &str);
    void SaveKnownDevices(File file);
    void LoadKnownDevices(File file, uint16_t version);
    void LoadString(File file, String &str);
    void LoadStringArray(File file, std::vector<String> &vstr);
    String settingsFile;
    uint16_t version;
    std::vector<KnownDevice> knownDevices;
    bool enableWhiteList;
};

extern Settings SettingsMngr;
#endif /*__SETTINGS_H__*/