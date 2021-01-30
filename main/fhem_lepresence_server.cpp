#include "config.h"
#if USE_FHEM_LEPRESENCE_SERVER
#include "main.h"
#include "fhem_lepresence_server.h"
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <Regexp.h>

#include "utility.h"
#include "SPIFFSLogger.h"

namespace FHEMLePresenceServer
{

  void publishTag(WiFiClient *client, char address[ADDRESS_STRING_SIZE + 5], unsigned long timeout,const char* reason )
  {
    char normalizedAddress[ADDRESS_STRING_SIZE];
    char msg[100] = "absence;rssi=unreachable;daemon="GATEWAY_NAME" V"VERSION;

    NormalizeAddress(address, normalizedAddress);

    CRITICALSECTION_READSTART(trackedDevicesMutex)
    for (auto &trackedDevice : BLETrackedDevices)
    {
      if (strcmp(normalizedAddress, trackedDevice.address) != 0)
        continue;

      if ((trackedDevice.lastDiscoveryTime + timeout) > NTPTime::seconds())
      {
        if (PUBLISH_BATTERY_LEVEL && trackedDevice.batteryLevel > 0)
          snprintf(msg, 100, "present;device_name=%s;rssi=%d;batteryPercent=%d;daemon="GATEWAY_NAME" V"VERSION, address, trackedDevice.rssiValue, trackedDevice.batteryLevel);
        else
          snprintf(msg, 100, "present;device_name=%s;rssi=%d;daemon="GATEWAY_NAME" V"VERSION, address, trackedDevice.rssiValue);
      }
      break;
    }
    CRITICALSECTION_READEND

    DEBUG_PRINTF("%s : %s\n", reason, msg);

    try
    {
      client->println(msg);
    }
    catch (...)
    {
      LOG_TO_FILE_E("Error: Cought exception sending message to FHEM client");
    }
  }

  void handleClient(void *pvParameters)
  {
    const uint16_t buffLen = 64;
    uint8_t buf[buffLen];
    char normalizedAddress[ADDRESS_STRING_SIZE];
    char address[ADDRESS_STRING_SIZE + 5];
    unsigned long timeout = MAX_NON_ADV_PERIOD;
    unsigned long lastreport = 0;
    address[0] = '\0';
    normalizedAddress[0] = '\0';

    WiFiClient *client = static_cast<WiFiClient *>(pvParameters);
    
    try
    {
      while (client->connected())
      {
        bool publish = false;
        bool fastDiscovery = false;
        char* reason = nullptr;
        if (client->available())
        {
          memset(buf, 0, buffLen);
          int read = client->read(buf, buffLen);
          if (read > 0)
          {
            DEBUG_PRINTLN((char *)buf);
            if (buf[read - 1] == '\n')
              buf[read - 1] = '\0';
            MatchState ms((char *)buf);
            if (ms.Match(R"(^%s*(%x%x:%x%x:%x%x:%x%x:%x%x:%x%x)%s*|%s*(%d+)%s*$)") == REGEXP_MATCHED)
            {
              ms.GetCapture(address, 0);
              char matchTimeout[10];
              ms.GetCapture(matchTimeout, 1);
              timeout = atoi(matchTimeout); 
              NormalizeAddress(address, normalizedAddress);
              FastDiscovery[normalizedAddress] = false;
              reason = "on request";
              publish = true;
            }
            else if (ms.Match(R"(^%s*now%s*$)") == REGEXP_MATCHED)
            {
              reason = "forced request";
              publish = true;
            }
          }
        }
        else if (address[0] != '\0')
        {
          fastDiscovery = (FastDiscovery.find(normalizedAddress) != FastDiscovery.end()) && FastDiscovery[normalizedAddress];
          if ((lastreport + timeout) < NTPTime::seconds() || fastDiscovery)
          {
            publish = true;
            if (fastDiscovery)
            {
                FastDiscovery[normalizedAddress] = false;
                reason = "fast discovery";
            } else
              reason = "periodic report";
          }
        }

        if (address[0] != '\0' && publish)
        {
          publishTag(client, address, timeout, reason);
          lastreport = NTPTime::seconds();
          if(fastDiscovery)
            delay(1000);
        }

        delay(100);
      }
    }
    catch (...)
    {
      LOG_TO_FILE_E("Error: Cought exception handling FHEM client");
    }

    DEBUG_PRINTLN("Client Disconnect");
    client->stop();
    delete client;
    vTaskDelete(NULL);
  }

  WiFiServer server(5333, 10); // Port, Maxclients
  std::map<TaskHandle_t, std::string> tasks;
  void wifiTask(void *pvParameters)
  {
    server.begin();
    for (;;)
    {
      esp_task_wdt_reset();

      WiFiClient client = server.available();
      if (client)
      {
        WiFiClient *pClient = new WiFiClient(); //<-- Will be deleted when the client disconnects
        *pClient = client;
        char taskName[50];
        snprintf(taskName, 50, "clientTask_%s:%d", pClient->remoteIP().toString().c_str(), pClient->remotePort());
        DEBUG_PRINTF("New connection from %s\n", (taskName + 11));
        LOG_TO_FILE_I("New connection from %s", (taskName + 11));

        TaskHandle_t h_task;
        xTaskCreate(handleClient, taskName, 3000, pClient, 2, &h_task);
        tasks[h_task] = taskName;
      }
      else
      {
        delay(500);
      }
    }
  }

  void Start()
  {
    TaskHandle_t h_task;
    xTaskCreatePinnedToCore(wifiTask, "wifiTask", 3000, NULL, 1 | portPRIVILEGE_BIT, &h_task, 1);
    tasks[h_task] = "wifiTask";
  }

  void RemoveCompletedTasks()
  {
    for (auto it = tasks.begin(); it != tasks.end();)
    {
      if (eTaskGetState(it->first) == eReady && it->second.substr(0, 6) == "client")
        it = tasks.erase(it);
      else
      {
        ++it;
      }
    }
  }

} // namespace FHEMLePresenceServer
#endif /*USE_FHEM_LEPRESENCE_SERVER*/
