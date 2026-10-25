#include "config.h"
#if USE_FHEM_LEPRESENCE_SERVER
#include "main.h"
#include "fhem_lepresence_server.h"
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <ctype.h>
#include <errno.h>
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

  char *TrimWhitespace(char *input)
  {
    while (isspace(static_cast<unsigned char>(*input)))
      input++;

    size_t length = strlen(input);
    while (length > 0 && isspace(static_cast<unsigned char>(input[length - 1])))
      input[--length] = '\0';

    return input;
  }

  bool ParseRequest(char *input, BleDeviceId &deviceId, char *requestedDeviceId,
                    size_t requestedDeviceIdSize, unsigned long &timeout)
  {
    input = TrimWhitespace(input);
    char *separator = strchr(input, '|');
    if (separator == nullptr || strchr(separator + 1, '|') != nullptr)
      return false;

    *separator = '\0';
    char *deviceIdText = TrimWhitespace(input);
    char *timeoutText = TrimWhitespace(separator + 1);
    size_t timeoutLength = strlen(timeoutText);
    if (timeoutLength == 0 || timeoutLength > 10 || requestedDeviceIdSize == 0)
      return false;

    for (size_t i = 0; i < timeoutLength; i++)
    {
      if (!isdigit(static_cast<unsigned char>(timeoutText[i])))
        return false;
    }

    char *end = nullptr;
    errno = 0;
    unsigned long parsedTimeout = strtoul(timeoutText, &end, 10);
    size_t deviceIdLength = strlen(deviceIdText);
    if (errno == ERANGE || end == timeoutText || *end != '\0' ||
        deviceIdLength >= requestedDeviceIdSize || !ParseDeviceId(deviceIdText, deviceId))
      return false;

    memcpy(requestedDeviceId, deviceIdText, deviceIdLength + 1);
    for (size_t i = 0; requestedDeviceId[i] != '\0'; i++)
      requestedDeviceId[i] = static_cast<char>(tolower(static_cast<unsigned char>(requestedDeviceId[i])));
    timeout = parsedTimeout;
    return true;
  }

  bool IsNowRequest(const char *input)
  {
    return strcmp(input, "now") == 0;
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
      inputLength = 0;
      discardingLine = false;
      hasReportedState = false;
      reportedPresent = false;
      reportedRssi = -100;
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
    char inputBuffer[64];
    size_t inputLength;
    bool discardingLine;
    bool hasReportedState;
    bool reportedPresent;
    int8_t reportedRssi;
  };

  bool IsDevicePresent(const BLETrackedDevice &trackedDevice, unsigned long interval,
                       unsigned long now)
  {
    if (!trackedDevice.hasPreviousDiscovery)
      return false;

    unsigned long lastDiscovery = static_cast<unsigned long>(trackedDevice.lastDiscoveryTime);
    unsigned long previousDiscovery = static_cast<unsigned long>(trackedDevice.previousDiscoveryTime);
    return now - lastDiscovery <= interval && now - previousDiscovery <= interval;
  }

  void publishTag(FHEMClient &fhemClient, const char *reason, bool &isPresent,
                  int8_t &reportedRssi)
  {
    char msg[192] = "";
    unsigned long now = NTPTime::seconds();
    isPresent = false;
    reportedRssi = -100;

    CRITICALSECTION_READSTART(trackedDevicesMutex)
    for (auto &trackedDevice : BLETrackedDevices)
    {
      if (fhemClient.deviceId != trackedDevice.deviceId)
        continue;

      isPresent = IsDevicePresent(trackedDevice, fhemClient.timeout, now);
      if (isPresent)
      {
        reportedRssi = trackedDevice.lastSeenRssi;
#if PUBLISH_BATTERY_LEVEL
        if (SettingsMngr.InBatteryList(trackedDevice.deviceId) &&
            trackedDevice.batteryLevel >= 0 && trackedDevice.batteryLevel <= 100)
        {
          snprintf(msg, sizeof(msg),
                   "present;device_name=%s;rssi=%d;batteryPercent=%d;daemon=%s V" VERSION "\n",
                   fhemClient.requestedDeviceId, reportedRssi, trackedDevice.batteryLevel,
                   SettingsMngr.gateway.c_str());
        }
        else
#endif
        {
          snprintf(msg, sizeof(msg), "present;device_name=%s;rssi=%d;daemon=%s V" VERSION "\n",
                   fhemClient.requestedDeviceId, reportedRssi, SettingsMngr.gateway.c_str());
        }
      }
      else
        snprintf(msg, sizeof(msg), "absence;rssi=unreachable;daemon=%s V" VERSION "\n",
                 SettingsMngr.gateway.c_str());
      break;
    }
    CRITICALSECTION_READEND

    if (msg[0] == '\0')
      snprintf(msg, sizeof(msg), "absence;rssi=unreachable;daemon=%s V" VERSION "\n",
               SettingsMngr.gateway.c_str());

    DEBUG_PRINTF("%s (%s): %s\n", reason, fhemClient.requestedDeviceId, msg);
    if (fhemClient.mClient.print(msg) == 0)
      LOG_TO_FILE_E("Error: Failed to send report to FHEM client");
  }

  int readLine(FHEMClient &fhemClient, char *buffer, size_t bufferLength)
  {
    while (fhemClient.mClient.available())
    {
      int data = fhemClient.mClient.read();
      if (data < 0)
        break;

      if (data == '\n')
      {
        if (fhemClient.discardingLine)
        {
          fhemClient.discardingLine = false;
          fhemClient.inputLength = 0;
          LOG_TO_FILE_W("Ignoring oversized FHEM command");
          return -1;
        }

        size_t length = fhemClient.inputLength;
        if (length >= bufferLength)
          length = bufferLength - 1;
        memcpy(buffer, fhemClient.inputBuffer, length);
        buffer[length] = '\0';
        fhemClient.inputLength = 0;
        return 1;
      }

      if (data == '\r' || fhemClient.discardingLine)
        continue;

      if (fhemClient.inputLength >= bufferLength - 1)
      {
        fhemClient.discardingLine = true;
        fhemClient.inputLength = 0;
        continue;
      }
      fhemClient.inputBuffer[fhemClient.inputLength++] = static_cast<char>(data);
    }
    return 0;
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
        bool publish = false;
        const char *reason = nullptr;
        char buf[sizeof(fhemClient.inputBuffer)];
        int lineStatus = readLine(fhemClient, buf, sizeof(buf));
        if (lineStatus < 0)
          return;

        if (lineStatus > 0)
        {
          char *command = TrimWhitespace(buf);
          if (ParseRequest(command, fhemClient.deviceId,
                           fhemClient.requestedDeviceId, sizeof(fhemClient.requestedDeviceId),
                           fhemClient.timeout))
          {
            reason = "on request";
            publish = true;
            fhemClient.mClient.print("command accepted\n");
          }
          else if (IsNowRequest(command))
          {
            if (fhemClient.requestedDeviceId[0] != '\0')
            {
              reason = "forced request";
              publish = true;
              fhemClient.mClient.print("command accepted\n");
            }
            else
              LOG_TO_FILE_W("Ignoring FHEM now command without a registered device");
          }
          else if (strcmp(command, "stop") == 0)
          {
            fhemClient.mClient.print("no command running\n");
          }
          else if (strcmp(command, "ping") == 0)
          {
            size_t connectedClients = 0;
            for (int i = 0; i < maxNumberOfClients; i++)
            {
              if (!FHEMClientPool[i].mAvailable && FHEMClientPool[i].mClient.connected())
                connectedClients++;
            }

            size_t deviceCount = 0;
            CRITICALSECTION_READSTART(trackedDevicesMutex)
            deviceCount = BLETrackedDevices.size();
            CRITICALSECTION_READEND

            char response[192];
            snprintf(response, sizeof(response),
                     "pong [clients=%u;devices=%u;min_age=0;max_age=0;beacons_hcitool=0;beacons_hcidump=0;beacons_diff=0]\n",
                     static_cast<unsigned>(connectedClients), static_cast<unsigned>(deviceCount));
            fhemClient.mClient.print(response);
            fhemClient.mClient.flush();
            fhemClient.mClient.stop();
            RelaseFHEMClient(&fhemClient);
            return;
          }
          else
          {
            LOG_TO_FILE_W("Ignoring unsupported FHEM command: %s", command);
          }
        }

        if (fhemClient.requestedDeviceId[0] != '\0' && !publish)
        {
          bool currentPresence = false;
          int8_t currentRssi = -100;
          unsigned long now = NTPTime::seconds();
          CRITICALSECTION_READSTART(trackedDevicesMutex)
          for (auto &trackedDevice : BLETrackedDevices)
          {
            if (trackedDevice.deviceId == fhemClient.deviceId)
            {
              currentPresence = IsDevicePresent(trackedDevice, fhemClient.timeout, now);
              if (currentPresence)
                currentRssi = trackedDevice.lastSeenRssi;
              break;
            }
          }
          CRITICALSECTION_READEND

          bool presenceChanged = fhemClient.hasReportedState &&
                                 currentPresence != fhemClient.reportedPresent;
          int rssiDifference = static_cast<int>(currentRssi) - static_cast<int>(fhemClient.reportedRssi);
          if (rssiDifference < 0)
            rssiDifference = -rssiDifference;
          bool rssiChanged = currentPresence && fhemClient.reportedPresent && rssiDifference > 10;
          bool periodicReport = now - fhemClient.lastreport >= fhemClient.timeout;
          if (presenceChanged || rssiChanged || periodicReport)
          {
            publish = true;
            reason = presenceChanged ? "presence change" :
                     (rssiChanged ? "RSSI change" : "periodic report");
          }
        }

        if (fhemClient.requestedDeviceId[0] != '\0' && publish)
        {
          bool reportedPresent = false;
          int8_t reportedRssi = -100;
          publishTag(fhemClient, reason, reportedPresent, reportedRssi);
          fhemClient.hasReportedState = true;
          fhemClient.reportedPresent = reportedPresent;
          fhemClient.reportedRssi = reportedRssi;
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

  WiFiServer server(FHEM_LEPRESENCE_SERVER_PORT, maxNumberOfClients);

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
