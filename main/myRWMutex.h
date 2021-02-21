#ifndef MYRWMUTEX_H
#define MYRWMUTEX_H
#include "freertos/FreeRTOS.h"
#include "DebugPrint.h"
#include <WString.h>
#include <esp32-hal-log.h>

class MyRWMutex
{
public:
    MyRWMutex(const String& name=""){
         mReadMutex = xSemaphoreCreateRecursiveMutex(); 
         mWriteMutex = xSemaphoreCreateRecursiveMutex(); 
         mReaders = 0;
         mName = name;
         }
    ~MyRWMutex() { 
        vSemaphoreDelete(mReadMutex); 
         vSemaphoreDelete(mWriteMutex); 
        }

    void readLock()
    {
        log_i("Read Acquiring %s", mName.c_str());
        xSemaphoreTakeRecursive(mReadMutex, portMAX_DELAY);
        mReaders++;
        if(mReaders == 1)
            xSemaphoreTakeRecursive(mWriteMutex, portMAX_DELAY);
        xSemaphoreGiveRecursive(mReadMutex);
        log_i("Read Acquired %s", mName.c_str());
    }

    void readUnlock()
    {
        assert(mReaders > 0);
        log_i("Read Release %s", mName.c_str());
        xSemaphoreTakeRecursive(mReadMutex, portMAX_DELAY);
        mReaders--;
        if(mReaders == 0)
            xSemaphoreGiveRecursive(mWriteMutex);
        xSemaphoreGiveRecursive(mReadMutex);
    }

    void writeLock()
    {
        log_i("Write Acquiring %s", mName.c_str());
        xSemaphoreTakeRecursive(mWriteMutex, portMAX_DELAY);
        log_i("Write Acquired %s", mName.c_str());
    }

    void writeUnlock()
    {
        log_i("Write Release %s", mName.c_str());
        xSemaphoreGiveRecursive(mWriteMutex);
    }
    

private:
    SemaphoreHandle_t mReadMutex;
    SemaphoreHandle_t mWriteMutex;
    uint64_t mReaders;
    String mName;
};

class WriteLocker
{
public:
    WriteLocker(MyRWMutex& m):mutex(m)
    {
        mutex.writeLock();
    }

    ~WriteLocker()
    {
        mutex.writeUnlock();
    }
private:
    MyRWMutex& mutex;
};

class ReadLocker
{
public:
    ReadLocker(MyRWMutex& m):mutex(m)
    {
        mutex.readLock();
    }

    ~ReadLocker()
    {
        mutex.readUnlock();
    }
private:
    MyRWMutex& mutex;
};

#define CRITICALSECTION_READSTART(x) { ReadLocker __lock__(x);
#define CRITICALSECTION_READEND }

#define CRITICALSECTION_WRITESTART(x) { WriteLocker __lock__(x);
#define CRITICALSECTION_WRITEEND }
#endif