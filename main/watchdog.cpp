#include <esp32-hal-timer.h>
namespace Watchdog
{
    hw_timer_t *timer = NULL;

    void IRAM_ATTR resetModule()
    {
        ets_printf("INFO: Watchdog Reboot\n");
        esp_restart();
    }

    void Initialize()
    {
        timer = timerBegin(0, 80, true); //timer 0, div 80
        timerAttachInterrupt(timer, &resetModule, true);
        timerAlarmWrite(timer, 120000000, false); //set time in us 120000000 = 120 sec
        timerAlarmEnable(timer);                  //enable interrupt
    }

    void Feed()
    {
        timerWrite(timer, 0); //reset timer (feed watchdog)
    }
} // namespace WatchDog