
#include "firmwarever.h"
#include "build_defs.h"
#include "SPIFFS.h"
namespace Firmware{

const char BuildTime[] =
    {
        BUILD_YEAR_CH0, BUILD_YEAR_CH1, BUILD_YEAR_CH2, BUILD_YEAR_CH3,
        '-',
        BUILD_MONTH_CH0, BUILD_MONTH_CH1,
        '-',
        BUILD_DAY_CH0, BUILD_DAY_CH1,
        'T',
        BUILD_HOUR_CH0, BUILD_HOUR_CH1,
        ':',
        BUILD_MIN_CH0, BUILD_MIN_CH1,
        ':',
        BUILD_SEC_CH0, BUILD_SEC_CH1,
        '\0'};

String FullVersion()
{
    String ver=VERSION;
    ver+=" ";
    ver+=BuildTime;
    return ver;
}

void writeVersion()
{
    File file = SPIFFS.open("/version.txt", "w");
    if (file)
    {
        String ver = FullVersion();
        file.write((uint8_t *)ver.c_str(), ver.length() + 1);
        file.close();
    }
}

String readVersion()
{
    String ver;
    File file = SPIFFS.open("/version.txt", "r");
    if (file)
    {
        ver = file.readStringUntil('\0');
        file.close();
    }
    return ver;
}
}