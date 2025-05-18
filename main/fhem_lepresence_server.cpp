#include "config.h"
#if USE_FHEM_LEPRESENCE_SERVER
#include "main.h"
#include "fhem_lepresence_server.h"
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <Regexp.h>

#include "utility.h"
#include "SPIFFSLogger.h"
#include "WiFiManager.h"
#include "settings.h"
namespace FHEMLePresenceServer
{
  struct FHEMClient
  {
    FHEMClient()
    {
      Release();
    }

    void Release()
    {
      mAvailable = true;
      mClient = WiFiClient(); // the following line is a workaround for a memory leak bug in arduino
      address[0] = '\0';
      normalizedAddress[0] = '\0';
      timeout = SettingsMngr.maxNotAdvPeriod;
      lastreport = 0;
    }

    void AssignToClient(WiFiClient &client)
    {
      mClient = client;
      mAvailable = false;
    }

    WiFiClient mClient;
    bool mAvailable;
    char address[ADDRESS_STRING_SIZE + 5];
    char normalizedAddress[ADDRESS_STRING_SIZE];
    unsigned long timeout;
    unsigned long lastreport;
  };

  void publishTag(FHEMClient &fhemClient, const char *reason)
  {
    char msg[100] = "";
    snprintf(msg,100, "absence;rssi=unreachable;daemon=%s V" VERSION "\n", SettingsMngr.gateway);
    CRITICALSECTION_READSTART(trackedDevicesMutex)
    for (auto &trackedDevice : BLETrackedDevices)
    {
      if (strcmp(fhemClient.normalizedAddress, trackedDevice.address) != 0)
        continue;

      if ((trackedDevice.lastDiscoveryTime + fhemClient.timeout) >= NTPTime::seconds())
      {
        if (PUBLISH_BATTERY_LEVEL && trackedDevice.batteryLevel > 0)
          snprintf(msg, 100, "present;device_name=%s;rssi=%d;batteryPercent=%d;daemon=%s V" VERSION "\n", fhemClient.address, trackedDevice.rssiValue, trackedDevice.batteryLevel,SettingsMngr.gateway);
        else
          snprintf(msg, 100, "present;device_name=%s;rssi=%d;daemon=%s V" VERSION "\n", fhemClient.address, trackedDevice.rssiValue,SettingsMngr.gateway);
      }
      break;
    }
    CRITICALSECTION_READEND

    DEBUG_PRINTF("%s (%s): %s\n", reason, fhemClient.normalizedAddress, msg);

    try
    {
      fhemClient.mClient.print(msg);
    }
    catch (...)
    {
      LOG_TO_FILE_E("Error: Caught exception sending message to FHEM client");
    }
  }

  int readLine(WiFiClient *client, uint8_t *buff, size_t bufflen)
  {
    int byteRead = 0;
    uint8_t data;
    int res = 0;

    while ((res = client->read(&data, 1)) > 0 && (byteRead < (bufflen - 1)) && (data != '\n'))
    {
      buff[byteRead] = data;
      byteRead++;
    }
    buff[byteRead] = 0;
    return res > 0 ? byteRead : res;
  }

  const int maxNumberOfClients = 10;
  FHEMClient FHEMClientPool[maxNumberOfClients];

  void RelaseFHEMClient(FHEMClient *item)
  {
    item->Release();
  }

  FHEMClient *GetAvailabeFHEMClient()
  {
    FHEMClient *wlkrClnt = FHEMClientPool;
    for (int i = 0; i < maxNumberOfClients; i++, wlkrClnt++)
    {
      if (wlkrClnt->mAvailable)
        return wlkrClnt;
    }
    return nullptr;
  }

  void handleClient(FHEMClient &fhemClient)
  {
    try
    {
      if (fhemClient.mClient.connected())
      {
        const uint16_t buffLen = 64;
        uint8_t buf[buffLen];

        bool publish = false;
        bool fastDiscovery = false;
        char *reason = nullptr;
        if (fhemClient.mClient.available())
        {
          fhemClient.mClient.setTimeout(fhemClient.timeout);
          memset(buf, 0, buffLen);
          if (readLine(&(fhemClient.mClient), buf, buffLen) > 0)
          {
            DEBUG_PRINTLN((char *)buf);
            MatchState ms((char *)buf);
            if (ms.Match(R"(^%s*(%x%x:%x%x:%x%x:%x%x:%x%x:%x%x)%s*|%s*(%d+)%s*$)") == REGEXP_MATCHED)
            {
              ms.GetCapture(fhemClient.address, 0);
              char matchTimeout[10];
              ms.GetCapture(matchTimeout, 1);
              fhemClient.timeout = atoi(matchTimeout);
              if (fhemClient.timeout <= SettingsMngr.scanPeriod)
                fhemClient.timeout = SettingsMngr.scanPeriod + 1;

              NormalizeAddress(fhemClient.address, fhemClient.normalizedAddress);
              FastDiscovery[fhemClient.normalizedAddress] = false;
              reason = "on request";
              publish = true;
              fhemClient.mClient.print("command accepted\n");
            }
            else if (ms.Match(R"(^%s*now%s*$)") == REGEXP_MATCHED)
            {
              reason = "forced request";
              publish = true;
              fhemClient.mClient.print("command accepted\n");
            }
            else
            {
              fhemClient.mClient.print("command rejected\n");
            }
          }
        }
        else if (fhemClient.address[0] != '\0')
        {
          fastDiscovery = (FastDiscovery.find(fhemClient.normalizedAddress) != FastDiscovery.end()) && FastDiscovery[fhemClient.normalizedAddress];
          if ((fhemClient.lastreport + fhemClient.timeout) < NTPTime::seconds() || fastDiscovery)
          {
            publish = true;
            if (fastDiscovery)
            {
              FastDiscovery[fhemClient.normalizedAddress] = false;
              reason = "fast discovery";
            }
            else
              reason = "periodic report";
          }
        }

        if (fhemClient.address[0] != '\0' && publish)
        {
          publishTag(fhemClient, reason);
          fhemClient.lastreport = NTPTime::seconds();
        }
      }
      else
      {
        DEBUG_PRINTF("Client Disconnect %s:%d for device %s\n", fhemClient.mClient.remoteIP().toString().c_str(), fhemClient.mClient.remotePort(), fhemClient.address);
        fhemClient.mClient.flush();
        fhemClient.mClient.stop();
        RelaseFHEMClient(&fhemClient);
      }
    }
    catch (...)
    {
      const char* errMsg = "Error: Caught exception handling FHEM client";
      LOG_TO_FILE_E(errMsg);
      DEBUG_PRINTLN(errMsg);
    }
  }

  WiFiServer server(5333, maxNumberOfClients); // Port, Maxclients

  void ListenForClientConnection()
  {
    WiFiClient client = server.available();
    if (client)
    {
      FHEMClient *fhemClient = GetAvailabeFHEMClient();
      if (fhemClient == nullptr)
      {
        const char* errMsg = "Warning: Reached the maximum number of clients";
        DEBUG_PRINTLN(errMsg);
        LOG_TO_FILE_W(errMsg);
        client.stop();
      }
      else
      {
        if (client.available())
          client.setNoDelay(true);

        fhemClient->AssignToClient(client);
        DEBUG_PRINTF("New connection from %s:%d\n", client.remoteIP().toString().c_str(), client.remotePort());
        LOG_TO_FILE_D("New connection from %s:%d\n", client.remoteIP().toString().c_str(), client.remotePort());
      }
      client = WiFiClient(); // the following line is a workaround for a memory leak bug in arduino
    }
  }

  void loop()
  {
    ListenForClientConnection();
    for (int i = 0; i < maxNumberOfClients; i++)
    {
      if (!FHEMClientPool[i].mAvailable)
      {
        handleClient(FHEMClientPool[i]);
      }
    }
  }

  void initializeServer()
  {
    server.begin();
  }

  void serverTask(void *pvParameters)
  {
    DEBUG_PRINTLN("FHEM serverTask started...");
    for (;;)
    {
      loop();
      delay(250);
    }
  }

  void startAsyncServer()
  {
    xTaskCreatePinnedToCore(serverTask, "FHEMLePresenceServer::serverTask", 4096, NULL, 10, nullptr, 1);
  }

} // namespace FHEMLePresenceServer
#endif /*USE_FHEM_LEPRESENCE_SERVER*/
