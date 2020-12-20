#ifndef __SETTINGS_H__
#define __SETTINGS_H__
#include <SPIFFS.h>
#include <WString.h>
#include <vector>

class Settings
{
public:
    Settings(const String& fileName = "", bool emptyLists = false);
    void SettingsFile(const String& fileName){settingsFile = fileName;};
    String toJSON();
    static void StringListToArray(const String& whiteList, std::vector<String>&vStr);
    bool Save();
    void Load();
    void FactoryReset(bool emptyLists = false);
    String GetSettingsFile(){ return settingsFile; }
    bool IsTraceable(const String& value);
    bool InBatteryList(const String& value);
    std::size_t GetMaxNumOfTraceableDevices();
    void EnableWhiteList(bool enable);
    void AddDeviceToWhiteList(const String& mac, bool checkBattery);
    String mqttUser;
    String mqttPwd;
    String mqttServer;
    uint16_t mqttPort;
    uint32_t scanPeriod;
private:
    bool InWhiteList(const String& value, const std::vector<String>& whiteList);
    String ArrayToStringList(const std::vector<String>& whiteList);
    void SaveString(File file, const String& str);
    void SaveStringArray(File file, const std::vector<String>& vstr);
    void LoadString(File file, String& str);
    void LoadStringArray(File file, std::vector<String>& vstr);    
    String settingsFile;
    uint16_t version;
    std::vector<String> trackWhiteList;
    std::vector<String> batteryWhiteList;
    bool enableWhiteList;
};

extern Settings SettingsMngr;
#endif /*__SETTINGS_H__*/