#include "settings.h"
#include "config.h"
#include "DebugPrint.h"

#define CURRENT_SETTING_VERSION 1

Settings SettingsMngr;

String Settings::ArrayToStringList(const std::vector<String>& whiteList)
{
    String result;
    for(uint8_t j =0; j < whiteList.size();j++)
    {
      if( j!=0)
        result+=",";
      result += R"(")"+whiteList[j]+R"(")";
    }
    return result;
}

void Settings::StringListToArray(const String& whiteList, std::vector<String>&vStr)
{
    if(whiteList.isEmpty())
        return;

    const char* strwlkr = whiteList.c_str();
    const unsigned int curBuffSize = 13;
    char str[curBuffSize];
    unsigned int cPos = 0;
    DEBUG_PRINTLN("StringListToArray:");
    bool continueProcess;
    do
    {
        if(*strwlkr == ',' || *strwlkr == '\0' || *strwlkr == '\n')
        {
            str[cPos] = '\0';
            if(cPos > 0)
                vStr.emplace_back(str);
            cPos = 0;
            continueProcess = *strwlkr != '\0';
        }
        else if(cPos < (curBuffSize-1))
        {
            str[cPos] = *strwlkr;
            cPos++;
        }

        strwlkr++;

    } while (continueProcess);

}

Settings::Settings(const String& fileName):settingsFile(fileName), version(CURRENT_SETTING_VERSION)
{
    FactoryReset();
};

void Settings::FactoryReset()
{
    #ifdef BLE_TRACKER_WHITELIST
    trackWhiteList = {BLE_TRACKER_WHITELIST};
    enableWhiteList = true;
    #else
    enableWhiteList = false;
    #endif

    #ifdef BLE_BATTERY_WHITELIST
        batteryWhiteList = {BLE_BATTERY_WHITELIST};
    #endif

    mqttUser=MQTT_USERNAME;
    mqttPwd=MQTT_PASSWORD;
    mqttServer = MQTT_SERVER;
    mqttPort = MQTT_SERVER_PORT;
}

String Settings::toJavaScriptObj()
{
    String data ="{";
    data+= R"(version:)" + String(version) + R"(,)";
    data+= R"(mqtt_address:")" + mqttServer + R"(",)";
    data+= R"(mqtt_port:)" + String(mqttPort)+ ",";
    data+= R"(mqtt_usr:")" + mqttUser  + R"(",)";
    data+= R"(mqtt_pwd:")" +mqttPwd+ R"(",)";
    data+= R"(whiteList:)" + String(enableWhiteList ? "true" : "false") + R"(,)";
    #if PUBLISH_BATTERY_LEVEL
    data+= R"(btry_list: [)"+ ArrayToStringList(batteryWhiteList) + R"(],)";
    #endif

    data+= R"(trk_list: [)" + ArrayToStringList(trackWhiteList) + R"(])";
    data+="}";

    return data;
}

   
void Settings::SaveString(File file, const String& str)
{
    size_t strLen;
    strLen = str.length();
    file.write((uint8_t *)str.c_str(), strLen+1);
}

void Settings::SaveStringArray(File file, const std::vector<String>& vstr)
{
    size_t vstrLen;
    vstrLen = vstr.size();
    file.write((uint8_t *)&vstrLen, sizeof(vstrLen));
    for(auto& str : vstr)
        SaveString(file, str);
}

void Settings::LoadString(File file, String& str)
{
    str = file.readStringUntil('\0');
}

void Settings::LoadStringArray(File file, std::vector<String>& vstr)
{
    size_t vstrLen;
    file.read((uint8_t *)&vstrLen, sizeof(vstrLen));
    vstr.clear();
    for(size_t i = 0;i < vstrLen; i++)
    {
       vstr.emplace_back(file.readStringUntil('\0'));
    } 
}

bool Settings::Save()
{
    File file = SPIFFS.open(settingsFile, "w");
    if(file)
    {
        file.write((uint8_t *)&version, sizeof(Settings::version));
        SaveString(file,mqttServer);
        file.write((uint8_t *)&mqttPort, sizeof(mqttPort));
        SaveString(file,mqttUser);
        SaveString(file,mqttPwd);
        file.write((uint8_t *)&enableWhiteList, sizeof(enableWhiteList));
        SaveStringArray(file,batteryWhiteList);
        SaveStringArray(file,trackWhiteList);
        file.flush();
        file.close();
        return true;
    }

    return false;
}

void Settings::Load()
{
    File file = SPIFFS.open(settingsFile, "r");
    if(file)
    {
        file.read((uint8_t *)&version, sizeof(version));
        LoadString(file,mqttServer);
        file.read((uint8_t *)&mqttPort, sizeof(mqttPort));
        LoadString(file,mqttUser);
        LoadString(file,mqttPwd);
        file.read((uint8_t *)&enableWhiteList, sizeof(enableWhiteList));
        LoadStringArray(file,batteryWhiteList);
        LoadStringArray(file,trackWhiteList);
    }
}
