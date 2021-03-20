#include "settings.h"
#include "config.h"
#include "DebugPrint.h"

#define CURRENT_SETTING_VERSION 4

Settings SettingsMngr;

String Settings::ArrayToStringList(const std::vector<String> &whiteList)
{
    String result;
    for (uint8_t j = 0; j < whiteList.size(); j++)
    {
        if (j != 0)
            result += ",";
        result += R"(")" + whiteList[j] + R"(")";
    }
    return result;
}

void Settings::StringListToArray(const String &whiteList, std::vector<String> &vStr)
{
    if (whiteList.isEmpty())
        return;

    const char *strwlkr = whiteList.c_str();
    const unsigned int curBuffSize = 13;
    char str[curBuffSize];
    unsigned int cPos = 0;
    DEBUG_PRINTLN("StringListToArray:");
    bool continueProcess;
    do
    {
        if (*strwlkr == ',' || *strwlkr == '\0' || *strwlkr == '\n')
        {
            str[cPos] = '\0';
            if (cPos > 0)
                vStr.emplace_back(str);
            cPos = 0;
            continueProcess = *strwlkr != '\0';
        }
        else if (cPos < (curBuffSize - 1))
        {
            str[cPos] = *strwlkr;
            cPos++;
        }

        strwlkr++;

    } while (continueProcess);
}

Settings::Settings(const String &fileName, bool emptyLists) : settingsFile(fileName), version(CURRENT_SETTING_VERSION)
{
    FactoryReset(emptyLists);
};

void Settings::FactoryReset(bool emptyLists)
{
    if (emptyLists)
    {
        trackWhiteList.clear();
        batteryWhiteList.clear();
    }
    else
    {
#ifdef BLE_TRACKER_WHITELIST
        trackWhiteList = {BLE_TRACKER_WHITELIST};
#endif

#ifdef BLE_BATTERY_WHITELIST
        batteryWhiteList = {BLE_BATTERY_WHITELIST};
#endif
    }

    enableWhiteList = ENABLE_BLE_TRACKER_WHITELIST;

    mqttEnabled = USE_MQTT;
    mqttUser = MQTT_USERNAME;
    mqttPwd = MQTT_PASSWORD;
    mqttServer = MQTT_SERVER;
    mqttPort = MQTT_SERVER_PORT;
    scanPeriod = BLE_SCANNING_PERIOD;
    logLevel = DEFAULT_FILE_LOG_LEVEL;
    maxNotAdvPeriod = MAX_NON_ADV_PERIOD;
}

std::size_t Settings::GetMaxNumOfTraceableDevices()
{
    const std::size_t absoluteMaxNumOfTraceableDevices = 90;
    const std::size_t minNumOfTraceableDevices = std::min(trackWhiteList.size() + 1,absoluteMaxNumOfTraceableDevices);
    return enableWhiteList ? minNumOfTraceableDevices : absoluteMaxNumOfTraceableDevices;
}

void Settings::AddDeviceToWhiteList(const String &mac, bool checkBattery)
{
    if (!InWhiteList(mac, trackWhiteList))
        trackWhiteList.push_back(mac);

    if (checkBattery)
    {
        if (!InWhiteList(mac, batteryWhiteList))
            batteryWhiteList.push_back(mac);
    }
}

void Settings::EnableWhiteList(bool enable)
{
    enableWhiteList = enable;
}

String Settings::toJSON()
{
    String data = "{";
    data += R"("version":)" + String(version) + R"(,)";
    data += R"("mqtt_enabled":)";
    data += mqttEnabled ? "true" : "false";
    data += R"(,)";
    data += R"("mqtt_address":")" + mqttServer + R"(",)";
    data += R"("mqtt_port":)" + String(mqttPort) + ",";
    data += R"("mqtt_usr":")" + mqttUser + R"(",)";
    data += R"("mqtt_pwd":")" + mqttPwd + R"(",)";
    data += R"("scanPeriod":)" + String(scanPeriod) + ",";
    data += R"("maxNotAdvPeriod":)" + String(maxNotAdvPeriod) + ",";
    data += R"("loglevel":)"+ String(logLevel) + ",";
    data += R"("whiteList":)";
    data += (enableWhiteList ? "true" : "false");
    data += R"(,)";
    data += R"("trk_list":{)";
    bool first = true;
    for (auto &mac : trackWhiteList)
    {
        if (!first)
            data += ",";
        else
            first = false;
        data += R"(")" + mac + R"(":)" + (InBatteryList(mac) ? "true" : "false");
    }
    data += "}";
    data += "}";
    return data;
}

bool Settings::IsTraceable(const String &value)
{
    if (enableWhiteList)
        return InWhiteList(value, trackWhiteList);
    else
        return true;
}

bool Settings::InBatteryList(const String &value)
{
#if PUBLISH_BATTERY_LEVEL
    return InWhiteList(value, batteryWhiteList);
#else
    return false;
#endif
}

bool Settings::InWhiteList(const String &value, const std::vector<String> &whiteList)
{
    bool inWhiteList = false;
    for (uint8_t j = 0; j < whiteList.size(); j++)
        if (value == whiteList[j])
        {
            inWhiteList = true;
            break;
        }
    return inWhiteList;
}

void Settings::SaveString(File file, const String &str)
{
    size_t strLen;
    strLen = str.length();
    file.write((uint8_t *)str.c_str(), strLen + 1);
}

void Settings::SaveStringArray(File file, const std::vector<String> &vstr)
{
    size_t vstrLen;
    vstrLen = vstr.size();
    file.write((uint8_t *)&vstrLen, sizeof(vstrLen));
    for (auto &str : vstr)
        SaveString(file, str);
}

void Settings::LoadString(File file, String &str)
{
    str = file.readStringUntil('\0');
}

void Settings::LoadStringArray(File file, std::vector<String> &vstr)
{
    size_t vstrLen;
    file.read((uint8_t *)&vstrLen, sizeof(vstrLen));
    vstr.clear();
    for (size_t i = 0; i < vstrLen; i++)
    {
        vstr.emplace_back(file.readStringUntil('\0'));
    }
}

bool Settings::Save()
{
    File file = SPIFFS.open(settingsFile, "w");
    if (file)
    {
        file.write((uint8_t *)&version, sizeof(Settings::version));
        SaveString(file, mqttServer);
        file.write((uint8_t *)&mqttPort, sizeof(mqttPort));
        SaveString(file, mqttUser);
        SaveString(file, mqttPwd);
        file.write((uint8_t *)&enableWhiteList, sizeof(enableWhiteList));
        SaveStringArray(file, batteryWhiteList);
        SaveStringArray(file, trackWhiteList);
        file.write((uint8_t *)&scanPeriod, sizeof(scanPeriod));
        file.write((uint8_t *)&logLevel, sizeof(logLevel));
        file.write((uint8_t *)&maxNotAdvPeriod, sizeof(maxNotAdvPeriod));
        file.flush();
        file.close();
        return true;
    }

    return false;
}

void Settings::Load()
{
    File file = SPIFFS.open(settingsFile, "r");
    if (file)
    {
        uint16_t currVer;
        file.read((uint8_t *)&currVer, sizeof(currVer));
        LoadString(file, mqttServer);
        file.read((uint8_t *)&mqttPort, sizeof(mqttPort));
        LoadString(file, mqttUser);
        LoadString(file, mqttPwd);
        file.read((uint8_t *)&enableWhiteList, sizeof(enableWhiteList));
        LoadStringArray(file, batteryWhiteList);
        LoadStringArray(file, trackWhiteList);
        if (currVer > 1)
            file.read((uint8_t *)&scanPeriod, sizeof(scanPeriod));
        if (currVer > 2)
            file.read((uint8_t *)&logLevel, sizeof(logLevel));
        if (currVer > 3)
            file.read((uint8_t *)&maxNotAdvPeriod, sizeof(maxNotAdvPeriod));
    }
}

const std::vector<String>& Settings::GetTrackWhiteList()
{
    return trackWhiteList;
}
