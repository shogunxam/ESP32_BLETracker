#include "config.h"
#if USE_FHEM_LEPRESENCE_SERVER
#include "main.h"
#include "fhem_lepresence_server.h"
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <Regexp.h>
#include "myMutex.h"
#include "utility.h"

extern std::vector<BLETrackedDevice> BLETrackedDevices;
extern std::map<std::string, bool> FastDiscovery;
extern MyMutex trackedDevicesMutex;

namespace FHEMLePresenceServer
{

  void publishTag(WiFiClient *client, char address[ADDRESS_STRING_SIZE + 5], unsigned long timeout)
  {
    char normalizedAddress[ADDRESS_STRING_SIZE];
    char msg[100] = "absence;rssi=unreachable;daemon=BLETracker V" VERSION;
    NormalizeAddress(address, normalizedAddress);

    CRITICALSECTION_START(trackedDevicesMutex)
    for (auto &trackedDevice : BLETrackedDevices)
    {
      if (strcmp(normalizedAddress, trackedDevice.address) != 0)
        continue;

      DEBUG_PRINTF("$$$$$-> %lu + %lu < %lu\n", trackedDevice.lastDiscoveryTime, timeout, millis());
      if ((trackedDevice.lastDiscoveryTime + timeout) > millis())
      {
        if (PUBLISH_BATTERY_LEVEL && trackedDevice.batteryLevel > 0)
          snprintf(msg, 100, "present;device_name=%s;rssi=%d;batteryPercent=%d;daemon=BLETracker V%s", address, trackedDevice.rssiValue, trackedDevice.batteryLevel, VERSION);
        else
          snprintf(msg, 100, "present;device_name=%s;rssi=%d;daemon=BLETracker V%s", address, trackedDevice.rssiValue, VERSION);
      }
      break;
    }
    CRITICALSECTION_END

    DEBUG_PRINTLN(msg);
    try
    {
      client->println(msg);
    }
    catch (...)
    {
    }
    DEBUG_PRINTLN("$$$$$$$$$$ publishTag 3 ");
  }

  void handleClient(void *pvParameters)
  {
    const uint16_t buffLen = 64;
    uint8_t buf[buffLen];
    char address[ADDRESS_STRING_SIZE + 5];
    unsigned long timeout = MAX_NON_ADV_PERIOD;
    unsigned long lastreport = 0;
    short discoveryCounter = 0;

    address[0] = '\0';

    WiFiClient *client = static_cast<WiFiClient *>(pvParameters);
    try
    {
      while (client->connected())
      {
        bool publish = false;
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
            if (ms.Match(R"(^%s*([0-9a-fA-F][0-9a-fA-F]:[0-9a-fA-F][0-9a-fA-F]:[0-9a-fA-F][0-9a-fA-F]:[0-9a-fA-F][0-9a-fA-F]:[0-9a-fA-F][0-9a-fA-F]:[0-9a-fA-F][0-9a-fA-F])%s*|%s*(%d+)%s*$)") == REGEXP_MATCHED)
            {
              ms.GetCapture(address, 0);
              char matchTimeout[10];
              ms.GetCapture(matchTimeout, 1);
              timeout = atoi(matchTimeout) * 1000;
              publish = true;
            }
            else if (ms.Match(R"(^%s*now%s*$)") == REGEXP_MATCHED)
            {
              publish = true;
            }
          }
        }
        else if (address[0] != '\0')
        {
          char normalizedAddress[ADDRESS_STRING_SIZE];
          NormalizeAddress(address, normalizedAddress);
          bool fastDiscovery = (FastDiscovery.find(normalizedAddress) != FastDiscovery.end()) && FastDiscovery[normalizedAddress];
          if ((lastreport + timeout) > millis() || fastDiscovery)
          {
            publish = true;
            if (fastDiscovery)
              FastDiscovery[normalizedAddress] = false;
          }
          delay(100);
        }

        if (address[0] != '\0' && publish)
        {
          publishTag(client, address, timeout);
          lastreport = millis();
        }
      }
    }
    catch (...)
    {
    }

    DEBUG_PRINTLN("Client Disconnect");
    client->stop();
    delete (client);
    vTaskDelete(NULL);
  }

  WiFiServer server(5333, 16); // Port, Maxclients
  std::map<TaskHandle_t, std::string> tasks;
  void wifiTask(void *pvParameters)
  {
    server.begin();
    DEBUG_PRINTLN("$$$$$$$$$$ wifiTask Started $$$$$$$");
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

        TaskHandle_t h_task;
        xTaskCreate(handleClient, taskName, 2000, pClient, 2, &h_task);
        tasks[h_task] = taskName;
        DEBUG_PRINTLN("$$$$$$$$$$ wifiTask 1 ");
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
