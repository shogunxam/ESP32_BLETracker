#ifndef MYMUTEX_H
#define MYMUTEX_H
#include "freertos/FreeRTOS.h"
#include "DebugPrint.h"
#include <WString.h>
#include <esp32-hal-log.h>

class MyMutex
{
public:
    MyMutex(const String& name=""){ mMutex = xSemaphoreCreateRecursiveMutex(); mName = name;}
    ~MyMutex() { vSemaphoreDelete(mMutex); }
    void lock()
    {
        log_i("Acquiring %s", mName.c_str());
        xSemaphoreTakeRecursive(mMutex, portMAX_DELAY);
        log_i("Acquired %s", mName.c_str());
    }

    void unlock()
    {
        log_i("Release %s", mName.c_str());
        xSemaphoreGiveRecursive(mMutex);
    }
    
    bool try_lock()
    {
        log_i("Try Acquiring %s", mName.c_str());
        bool val = xSemaphoreTakeRecursive(mMutex, 0);
        log_i("Acquired %s", mName.c_str());
        return val;
    }

private:
    SemaphoreHandle_t mMutex;
    String mName;
};

class locker
{
public:
    locker(MyMutex& m):mutex(m)
    {
        mutex.lock();
    }

    ~locker()
    {
        mutex.unlock();
    }
private:
    MyMutex& mutex;
};

#define CRITICALSECTION_START(x) { locker __lock__(x);
#define CRITICALSECTION_END }
#endif