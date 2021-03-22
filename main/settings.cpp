#include "settings.h"
#include "config.h"
#include "DebugPrint.h"

#define CURRENT_SETTING_VERSION 5

Settings SettingsMngr;


Settings::KnownDevice::KnownDevice(const KnownDevice& dev)
{
    *this = dev;
}

Settings::KnownDevice::KnownDevice(const char* mac, bool batt, const char* desc)
{
    snprintf(address, ADDRESS_STRING_SIZE, "%s", mac); 
    snprintf(description, DESCRIPTION_STRING_SIZE, "%s", desc); 
    readBattery = batt;
}

Settings::KnownDevice::KnownDevice()
{
    address[0]='\0';
    description[0]='\0';
    readBattery= false;
}

Settings::KnownDevice& Settings::KnownDevice::operator=(const KnownDevice& dev)
{
    memcpy(address, dev.address, ADDRESS_STRING_SIZE);
    memcpy(description, dev.description, DESCRIPTION_STRING_SIZE);
    readBattery = dev.readBattery;
}

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
    KnownDevice d = { "CA67347FD139", true, "Nut Mario" };
    if (emptyLists)
    {
        knownDevices.clear();
    }
    else
    {
        knownDevices = {BLE_KNOWN_DEVICES_LIST};
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
    const std::size_t minNumOfTraceableDevices = std::min(knownDevices.size() + 1,absoluteMaxNumOfTraceableDevices);
    return enableWhiteList ? minNumOfTraceableDevices : absoluteMaxNumOfTraceableDevices;
}

void Settings::AddDeviceToList(const Settings::KnownDevice& device)
{
    if (!IsPropertyForDeviceEnabled(device.address, DeviceProperty::eTraceable))
        knownDevices.push_back(device);
}

void Settings::AddDeviceToList(const char mac[ADDRESS_STRING_SIZE], bool checkBattery, const char description[DESCRIPTION_STRING_SIZE])
{
    if (!IsPropertyForDeviceEnabled(mac, DeviceProperty::eTraceable))
    {
        KnownDevice device;
        memcpy(&device.address, mac, sizeof(device.address));
        memcpy(&device.description, description, sizeof(device.description));
        device.readBattery = checkBattery;
        knownDevices.push_back(std::move(device));
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
    for (auto &device : knownDevices)
    {
        if (!first)
            data += ",";
        else
            first = false;
        data += R"(")" + String(device.address) + R"(":{)";
        data += R"("battery":)";
        data += (device.readBattery ? "true" : "false");
        data += R"(,"desc":")" + String(device.description) + R"("})";
    }
    data += "}";
    data += "}";
    return data;
}

bool Settings::IsTraceable(const String &value)
{
    if (enableWhiteList)
        return IsPropertyForDeviceEnabled(value,DeviceProperty::eReadBattery);
    else
        return true;
}

bool Settings::InBatteryList(const String &value)
{
#if PUBLISH_BATTERY_LEVEL
    return IsPropertyForDeviceEnabled(value, DeviceProperty::eTraceable);
#else
    return false;
#endif
}

bool Settings::IsPropertyForDeviceEnabled(const String &value, DeviceProperty property)
{
   KnownDevice* device = nullptr;
    for (uint8_t j = 0; j < knownDevices.size(); j++)
    {
        if (value == knownDevices[j].address)
        {
            device = &knownDevices[j];
            break;
        }
    }

    if(device != nullptr)
    {
        switch (property)
        {
        case DeviceProperty::eTraceable:
            return true;
            break;
        case DeviceProperty::eReadBattery:
            return device->readBattery;
        default:
            return false;
        }
    }

    return false;
}

void Settings::SaveString(File file, const String &str)
{
    size_t strLen;
    strLen = str.length();
    file.write((uint8_t *)str.c_str(), strLen + 1);
}

void Settings::SaveKnownDevices(File file)
{
    size_t vstrLen;
    vstrLen = knownDevices.size();
    file.write((uint8_t *)&vstrLen, sizeof(vstrLen));
    for (auto &device : knownDevices)
    {
       file.write((uint8_t *)&device, sizeof(KnownDevice));
    }
}

void Settings::LoadKnownDevices(File file, uint16_t version)
{
    knownDevices.clear();
    if(version > 4)
    {
        size_t vstrLen;
        file.read((uint8_t *)&vstrLen, sizeof(vstrLen));
        for (size_t i = 0; i < vstrLen; i++)
        {
            KnownDevice device;
            file.read((uint8_t *)&device, sizeof(KnownDevice));
            knownDevices.push_back(device);
        }
    } 
    else //Build KnowDevices from 2 arrays
    {
        std::vector<String>batteryWhiteList;
        std::vector<String>trackWhiteList;
        LoadStringArray(file, batteryWhiteList);
        LoadStringArray(file, trackWhiteList);
        for(const auto& mac : trackWhiteList)
        {
            KnownDevice dev;
            dev.readBattery = false;
            dev.description[0] = '\0';
            snprintf(dev.address, ADDRESS_STRING_SIZE, "%s", mac.c_str()); 
            for(const auto& macBatt : batteryWhiteList)
            {
                if(mac == macBatt)
                {
                    dev.readBattery = true;
                    break;
                }
            }
            knownDevices.push_back(dev);
        }
    }
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
        SaveKnownDevices(file);
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
        LoadKnownDevices(file, currVer);
        if (currVer > 1)
            file.read((uint8_t *)&scanPeriod, sizeof(scanPeriod));
        if (currVer > 2)
            file.read((uint8_t *)&logLevel, sizeof(logLevel));
        if (currVer > 3)
            file.read((uint8_t *)&maxNotAdvPeriod, sizeof(maxNotAdvPeriod));
    }
}

const std::vector<Settings::KnownDevice>& Settings::GetKnownDevicesList()
{
    return knownDevices;
}
