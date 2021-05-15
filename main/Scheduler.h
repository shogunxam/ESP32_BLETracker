#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_
#define _TASK_PRIORITY          // Support for layered scheduling priority
#define _TASK_STD_FUNCTION      // Support for std::function (ESP8266 and ESP32 ONLY)
#include <TaskSchedulerDeclarations.h>
namespace scheduler
{
    Scheduler& get();
}
#endif /*_SCHEDULER_H_*/