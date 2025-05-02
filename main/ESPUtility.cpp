#include <esp_timer.h>
#include  "SPIFFSLogger.h"

namespace ESPUtility {

  #include <esp_timer.h>

  void IRAM_ATTR resetTimerCallback(void* arg)
  {
      LOG_TO_FILE_I("Executing scheduled restart");      
      esp_restart();
  }
  
  void scheduleReset(uint32_t delayMs)
  {
      esp_timer_handle_t resetTimer;
      esp_timer_create_args_t timerArgs = {
          .callback = &resetTimerCallback,
          .arg = NULL,
          .name = "reset_timer"
      };
      
      LOG_TO_FILE_I("Restart scheduled in %u milliseconds", delayMs);
      
      if (esp_timer_create(&timerArgs, &resetTimer) == ESP_OK) {
          esp_timer_start_once(resetTimer, delayMs * 1000); // Convert ms in μs
      } else {
          LOG_TO_FILE_E("Unable to create restart timer");
      }
  }
}