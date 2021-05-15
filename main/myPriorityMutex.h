#ifndef __MYPRIORITYMUTEX_H__
#define __MYPRIORITYMUTEX_H__
#include <freertos/FreeRTOS.h>
#include <esp_system.h>
#include "DebugPrint.h"
#include <queue>
class MyPriorityMutex
{
    public:

    MyPriorityMutex()
    {
         mGuard = xSemaphoreCreateRecursiveMutex(); 
         mEventGroup = xEventGroupCreate();
         mCurrentPrio = 0;
    }

    void lock(uint8_t threadID)
    {
        xSemaphoreTakeRecursive(mGuard, portMAX_DELAY);
        mQueue.push(threadID);
        xSemaphoreGiveRecursive(mGuard);

        
        while(true)
        {
            xSemaphoreTakeRecursive(mGuard, portMAX_DELAY);
            if(mQueue.front() == threadID)
                break;
            xSemaphoreGiveRecursive(mGuard);
            xEventGroupWaitBits(mEventGroup,BIT0, true,true, portMAX_DELAY);
            xEventGroupClearBits(mEventGroup,BIT0);
        }
    }

    void unlock(uint8_t threadID)
    {
        xSemaphoreTakeRecursive(mGuard, portMAX_DELAY);
        mQueue.pop();
        xSemaphoreGiveRecursive(mGuard);
        xEventGroupSetBits(mEventGroup,BIT0);
    }

    private:
    std::queue<uint8_t> mQueue;
    EventGroupHandle_t mEventGroup;
    SemaphoreHandle_t mGuard;
    uint8_t mCurrentPrio;
};
#endif /*__MYPRIORITYMUTEX_H__*/