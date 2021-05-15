#ifndef _BLE_TASKS_H_
#define _BLE_TASKS_H_
#include "main.h"
#include "settings.h"
namespace BLETask
{

void Initialize();
void Scan();
bool ScanCompleted();
void ReadBatteryForDevices( void(*callback)(void), bool progressiveMode = false);
void ForceBatteryRead(const char *normalizedAddr);
}
#endif