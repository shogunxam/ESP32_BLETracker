#include <esp32-hal-timer.h>
#include <atomic>
#include "DebugPrint.h"
#include "SPIFFSLogger.h"

namespace Watchdog
{
    static std::atomic_ulong WatchDogStartTime(0);
    void WatchDogloop(void *)
    {
        WatchDogStartTime.store(millis());
        while (true)
        {
            if (millis() > (WatchDogStartTime.load() + 120000))
            {
                const char *errMsg = "Error: Watchdog Reboot";
                DEBUG_PRINTLN(errMsg);
                LOG_TO_FILE_E(errMsg);
                delay(100);
                esp_restart();
            }
            delay(1000);
        };
    }

    void Feed()
    {
        WatchDogStartTime.store(millis());
    }

    void Initialize()
    {
        xTaskCreatePinnedToCore(
            WatchDogloop,   /* Function to implement the task */
            "WatchDogloop", /* Name of the task */
            3072,           /* Stack size in words */
            NULL,           /* Task input parameter */
            20,             /* Priority of the task */
            NULL,           /* Task handle. */
            1);
    }
} // namespace Watchdog
