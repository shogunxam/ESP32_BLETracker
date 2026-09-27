#include "config.h"
#if USE_FHEM_LEPRESENCE_SERVER
#include "main.h"
#include "fhem_lepresence_server.h"
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <ctype.h>
#include <stdlib.h>

#include "utility.h"
#include "SPIFFSLogger.h"
#include "WiFiManager.h"
#include "settings.h"
namespace FHEMLePresenceServer
{
  bool ParseDeviceId(const char *input, BleDeviceId &deviceId)
  {
    char normalized[BleDeviceId::UUID_STRING_SIZE];
    size_t normalizedLength = 0;

    for (size_t i = 0; input[i] != '\0'; i++)
    {
      if (input[i] == ':' || input[i] == '-')
        continue;

      if (!isxdigit(static_cast<unsigned char>(input[i])) ||
          normalizedLength >= BleDeviceId::UUID_STRING_SIZE - 1)
        return false;

      normalized[normalizedLength++] = input[i];
    }

    if (normalizedLength != BleDeviceId::MAC_ID_SIZE * 2 &&
        normalizedLength != BleDeviceId::UUID_STRING_SIZE - 1)
      return false;

    normalized[normalizedLength] = '\0';
    deviceId = BleDeviceId(normalized);
    return true;
  }

  bool ParseRequest(const char *input, BleDeviceId &deviceId, char *requestedDeviceId,
                    size_t requestedDeviceIdSize, unsigned long &timeout)
  {
    char deviceIdText[BleDeviceId::UUID_STRING_SIZE];
    char timeoutText[11];
    char extra;

    if (sscanf(input, "%40s %10s %c", deviceIdText, timeoutText, &extra) != 2)
      return false;

    for (size_t i = 0; timeoutText[i] != '\0'; i++)
    {
      if (!isdigit(static_cast<unsigned char>(timeoutText[i])))
        return false;
    }

    char *end = nullptr;
    unsigned long parsedTimeout = strtoul(timeoutText, &end, 10);
    if (end == timeoutText || *end != '\0' || !ParseDeviceId(deviceIdText, deviceId))
      return false;

    strncpy(requestedDeviceId, deviceIdText, requestedDeviceIdSize - 1);
    requestedDeviceId[requestedDeviceIdSize - 1] = '\0';
    timeout = parsedTimeout;
    return true;
  }

  bool IsNowRequest(const char *input)
  {
    while (isspace(static_cast<unsigned char>(*input)))
      input++;

    if (strncmp(input, "now", 3) != 0)
      return false;

    input += 3;
    while (isspace(static_cast<unsigned char>(*input)))
      input++;
    return *input == '\0';
  }

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
      requestedDeviceId[0] = '\0';
      deviceId = BleDeviceId();
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
    char requestedDeviceId[BleDeviceId::UUID_STRING_SIZE];
    BleDeviceId deviceId;
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
        if (fhemClient.deviceId != trackedDevice.deviceId)
        continue;

      if ((trackedDevice.lastDiscoveryTime + fhemClient.timeout) >= NTPTime::seconds())
      {
        if (PUBLISH_BATTERY_LEVEL && trackedDevice.batteryLevel > 0)
          snprintf(msg, 100, "present;device_name=%s;rssi=%d;batteryPercent=%d;daemon=%s V" VERSION "\n", fhemClient.requestedDeviceId, trackedDevice.rssiValue, trackedDevice.batteryLevel,SettingsMngr.gateway);
        else
          snprintf(msg, 100, "present;device_name=%s;rssi=%d;daemon=%s V" VERSION "\n", fhemClient.requestedDeviceId, trackedDevice.rssiValue,SettingsMngr.gateway);
      }
      break;
    }
    CRITICALSECTION_READEND

    DEBUG_PRINTF("%s (%s): %s\n", reason, fhemClient.requestedDeviceId, msg);

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
    if (discoveryMode)
      return;
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
              if (ParseRequest((char *)buf, fhemClient.deviceId,
                               fhemClient.requestedDeviceId, sizeof(fhemClient.requestedDeviceId),
                               fhemClient.timeout))
            {
              if (fhemClient.timeout <= SettingsMngr.scanPeriod)
                fhemClient.timeout = SettingsMngr.scanPeriod + 1;

                FastDiscovery[fhemClient.deviceId] = false;
              reason = "on request";
              publish = true;
              fhemClient.mClient.print("command accepted\n");
            }
              else if (IsNowRequest((char *)buf))
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
        else if (fhemClient.requestedDeviceId[0] != '\0')
        {
            fastDiscovery = (FastDiscovery.find(fhemClient.deviceId) != FastDiscovery.end()) && FastDiscovery[fhemClient.deviceId];
          if ((fhemClient.lastreport + fhemClient.timeout) < NTPTime::seconds() || fastDiscovery)
          {
            publish = true;
            if (fastDiscovery)
            {
                FastDiscovery[fhemClient.deviceId] = false;
              reason = "fast discovery";
            }
            else
              reason = "periodic report";
          }
        }

        if (fhemClient.requestedDeviceId[0] != '\0' && publish)
        {
          publishTag(fhemClient, reason);
          fhemClient.lastreport = NTPTime::seconds();
        }
      }
      else
      {
        DEBUG_PRINTF("Client Disconnect %s:%d for device %s\n", fhemClient.mClient.remoteIP().toString().c_str(), fhemClient.mClient.remotePort(), fhemClient.requestedDeviceId);
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
