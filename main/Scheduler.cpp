#include "Scheduler.h"
namespace scheduler
{
    static Scheduler runner;
    Scheduler& get(){return runner;};
}