#ifndef __FHEM_LEPRESENCE_SERVER_H__
#define __FHEM_LEPRESENCE_SERVER_H__
#include <map>
#include <esp_task_wdt.h>

namespace FHEMLePresenceServer
{
    void Start();
    void RemoveCompletedTasks();
};
#endif /*__FHEM_LEPRESENCE_SERVER_H__*/