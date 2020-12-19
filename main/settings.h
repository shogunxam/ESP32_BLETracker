#ifndef __SETTINGS_H__
#define __SETTINGS_H__
#include <SPIFFS.h>
#include <WString.h>
#include <vector>

class Settings
{
public:
    Settings(const String& fileName = "");
    void SettingsFile(const String& fileName){settingsFile = fileName;};
    String toJavaScriptObj();
    static void StringListToArray(const String& whiteList, std::vector<String>&vStr);
    bool Save();
    void Load();
    void FactoryReset();
    String GetSettingsFile(){ return settingsFile; }
    String mqttUser;
    String mqttPwd;
    String mqttServer;
    uint16_t mqttPort;
    std::vector<String> trackWhiteList;
    std::vector<String> batteryWhiteList;
    bool enableWhiteList;
private:
    String ArrayToStringList(const std::vector<String>& whiteList);
    void SaveString(File file, const String& str);
    void SaveStringArray(File file, const std::vector<String>& vstr);
    void LoadString(File file, String& str);
    void LoadStringArray(File file, std::vector<String>& vstr);

    void fromJavaScriptObj(const String& data);
    String settingsFile;
    uint16_t version;
};

extern Settings SettingsMngr;
#endif /*__SETTINGS_H__*/