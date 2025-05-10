#include <atomic>
#include "config.h"
#if USE_MQTT
#include "mqtt_client.h"
#include "DebugPrint.h"
#include "SPIFFSLogger.h"
#include "WiFiManager.h"
#include "watchdog.h"
#include "settings.h"
#include "utility.h"
#include "BLEScanTask.h"
#include "firmwarever.h"

// MQTT_KEEPALIVE : keepAlive interval in Seconds
// Keepalive timeout for default MQTT Broker is 10s
#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE 30
#endif

// MQTT_SOCKET_TIMEOUT: socket timeout interval in Seconds
#ifndef MQTT_SOCKET_TIMEOUT
#define MQTT_SOCKET_TIMEOUT 60
#endif

#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient
namespace MQTTClient
{

  PubSubClient mqttClient(WiFiManager::GetWiFiClient());

  ///////////////////////////////////////////////////////////////////////////
  //   MQTT
  ///////////////////////////////////////////////////////////////////////////
  volatile unsigned long lastMQTTConnection = 0;
  /*
    Function called to connect/reconnect to the MQTT broker
  */
  static bool firstTimeMQTTConnection = true;
  static bool MQTTConnectionErrorLogged = false;
  static uint32_t MQTTErrorCounter = 0;
  static const size_t cMaxLocationNameLength = 32;
  static const size_t cMaxGatewayNameLength = 32;
  static const size_t cMQTTBaseSensorTopicLength = cMaxLocationNameLength + cMaxGatewayNameLength + 2; // +2 for "/"

  const char *getMQTTBaseSensorTopic()
  {
    static char MQTT_BASE_SENSOR_TOPIC[cMQTTBaseSensorTopicLength] = "";
    if (MQTT_BASE_SENSOR_TOPIC[0] == 0)
    {
      char location[cMaxLocationNameLength] = "";
      char gateway[cMaxGatewayNameLength] = "";
      strncpy(location, LOCATION, sizeof(location));
      location[sizeof(location) - 1] = '\0'; // Ensure null-termination
      strncpy(gateway, SettingsMngr.gateway.c_str(), sizeof(gateway));
      gateway[sizeof(gateway) - 1] = '\0'; // Ensure null-termination

      snprintf(MQTT_BASE_SENSOR_TOPIC, sizeof(MQTT_BASE_SENSOR_TOPIC), "%s/%s", location, gateway);
    }

    return MQTT_BASE_SENSOR_TOPIC;
  }

  const char *getMQTTAvailabilityTopic()
  {
    static char MQTT_AVAILABILITY_TOPIC[cMQTTBaseSensorTopicLength + 10] = "";
    if (MQTT_AVAILABILITY_TOPIC[0] == 0)
    {

      snprintf(MQTT_AVAILABILITY_TOPIC, sizeof(MQTT_AVAILABILITY_TOPIC), "%s/LWT", getMQTTBaseSensorTopic());
    }
    return MQTT_AVAILABILITY_TOPIC;
  }

  const char *getMQTTSysInfoTopic()
  {
    static char MQTT_SYSINFO_TOPIC[cMQTTBaseSensorTopicLength + 10] = "";
    if (MQTT_SYSINFO_TOPIC[0] == 0)
    {
      snprintf(MQTT_SYSINFO_TOPIC, sizeof(MQTT_SYSINFO_TOPIC), "%s/sysinfo", getMQTTBaseSensorTopic());
    }
    return MQTT_SYSINFO_TOPIC;
  }

  /*
    Function called to publish to a MQTT topic with the given payload
  */
  void _publishToMQTT(const char *topic, const char *payload, bool retain)
  {
    if (mqttClient.publish(topic, payload, retain))
    {
      DEBUG_PRINTF("INFO: MQTT message published successfully, topic: %s , payload: %s , retain: %s \n", topic, payload, retain ? "True" : "False");
    }
    else
    {
      DEBUG_PRINTF("ERROR: MQTT message not published, either connection lost, or message too large. Topic: %s , payload: %s , retain: %s \n", topic, payload, retain ? "True" : "False");
      LOG_TO_FILE_E("Error: MQTT message not published");
    }
  }

  void MQTTLoopTask(void *parameter)
  {
    const unsigned long AVAILABILITY_UPDATE_INTERVAL = 300; // 5 minutes
    unsigned long lastMQTTAvailabilityUpdate = NTPTime::seconds();
    for (;;)
    {
      if (mqttClient.connected())
      {
        mqttClient.loop();
        if ((NTPTime::seconds() - lastMQTTAvailabilityUpdate) > AVAILABILITY_UPDATE_INTERVAL)
        {
          _publishToMQTT(getMQTTAvailabilityTopic(), MQTT_PAYLOAD_AVAILABLE, true);
          lastMQTTAvailabilityUpdate = NTPTime::seconds();
        }
      }
      else
      {
        // Reconnect to MQTT broker if not connected
        if (WiFi.status() == WL_CONNECTED && !WiFiManager::IsAccessPointModeOn())
        {
          DEBUG_PRINTLN("MQTT disconnected, attempting reconnection...");
          connectToMQTT();
        }
      }

      delay(1000); // Run the loop every second
    }
  }

  void initializeMQTT()
  {
    mqttClient.setServer(SettingsMngr.serverAddr.c_str(), SettingsMngr.serverPort);
    xTaskCreatePinnedToCore(
        MQTTLoopTask,
        "MQTTLoopTask",
        4096,
        NULL,
        1,
        NULL,
        0);
  }

  static std::atomic<bool> mqttConnectionInProgress(false);
  bool connectToMQTT()
  {
    if (mqttClient.connected())
    {
      return true;
    }

    bool expected = false;
    if (!mqttConnectionInProgress.compare_exchange_strong(expected, true))
    {
      // Another thread is already trying to connect
      delay(300);
      return mqttClient.connected();
    }

    // Check if WiFi is connected and in case connect to it
    if (!WiFiManager::WiFiConnect(SettingsMngr.wifiSSID, SettingsMngr.wifiPwd) || WiFiManager::IsAccessPointModeOn())
    {
      DEBUG_PRINTLN("ERROR: WiFi not connected, cannot connect to MQTT broker");
      mqttConnectionInProgress = false;
      return false;
    }

    uint8_t maxRetry = 3; // Retry 3 times
    while (!mqttClient.connected() && maxRetry > 0)
    {
      DEBUG_PRINTF("INFO: Connecting to MQTT broker: %s\n", SettingsMngr.serverAddr.c_str());

      const char *errMsg = "Error: MQTT broker disconnected, connecting...";
      DEBUG_PRINTLN(errMsg);
      if (!firstTimeMQTTConnection && !MQTTConnectionErrorLogged)
      {
        LOG_TO_FILE_E(errMsg);
      }

      String clientId = SettingsMngr.gateway + "-" + String(WiFi.macAddress()); // Ensure the client ID is unique
      if (mqttClient.connect(clientId.c_str(), SettingsMngr.serverUser.c_str(), SettingsMngr.serverPwd.c_str(), getMQTTAvailabilityTopic(), 1, true, MQTT_PAYLOAD_UNAVAILABLE))
      {
        DEBUG_PRINTLN(F("INFO: The client is successfully connected to the MQTT broker"));
        LOG_TO_FILE_I("MQTT broker connected!");
        MQTTConnectionErrorLogged = false;
        _publishToMQTT(getMQTTAvailabilityTopic(), MQTT_PAYLOAD_AVAILABLE, true);
      }
      else
      {
        DEBUG_PRINTLN(F("ERROR: The connection to the MQTT broker failed"));
        DEBUG_PRINTF("INFO: MQTT username: %s\n", SettingsMngr.serverUser.c_str())
        DEBUG_PRINTF("INFO: MQTT password: %s\n", SettingsMngr.serverPwd.c_str());
        DEBUG_PRINTF("INFO: MQTT broker: %s\n", SettingsMngr.serverAddr.c_str());
#if ENABLE_FILE_LOG
        // To prevent MQTT error logs from saturating the limited storage space,
        // repeted connection errors are logged only after 500 other logs.
        // This ensures space is preserved for documenting other system issues.

        uint32_t numLogs = SPIFFSLogger.numOfLogsPerSession();
        uint32_t deltaLogs;

        // Calculate the number of logs since the last MQTT error considering the wrap-around
        deltaLogs = (numLogs < MQTTErrorCounter) ? (~uint32_t(0) - MQTTErrorCounter + numLogs) : (numLogs - MQTTErrorCounter);

        // MQTTConnectionErrorLogged is reset if the number of logs since the last error is greater than 500
        // or the MQTT connection is successful
        if (deltaLogs >= 500)
        {
          MQTTConnectionErrorLogged = false;
        }

        if (!MQTTConnectionErrorLogged)
        {
          LOG_TO_FILE_E("Error: Connection to the MQTT broker failed!");
          MQTTConnectionErrorLogged = true;
          MQTTErrorCounter = SPIFFSLogger.numOfLogsPerSession();
        }
#endif
      }

      Watchdog::Feed();
      maxRetry--;
    }

    mqttConnectionInProgress = false;
    firstTimeMQTTConnection = false;
    return mqttClient.connected();
  }

  void publishToMQTT(const char *topic, const char *payload, bool retain)
  {
    if (connectToMQTT())
    {
      _publishToMQTT(topic, payload, retain);
      delay(100);
    }
  }

  void publishAvailabilityToMQTT()
  {
    if (NTPTime::seconds() > lastMQTTConnection)
    {
      lastMQTTConnection = NTPTime::seconds() + MQTT_CONNECTION_TIME_OUT;
      DEBUG_PRINTF("INFO: MQTT availability topic: %s\n", getMQTTAvailabilityTopic());
      publishToMQTT(getMQTTAvailabilityTopic(), MQTT_PAYLOAD_AVAILABLE, true);

#if ENABLE_HOME_ASSISTANT_MQTT_DISCOVERY
      publishTrackerStatus();
      publishDevicesList();
#endif // ENABLE_HOME_ASSISTANT_MQTT_DISCOVERY
    }
  }

  static const char *devicePayloadBtt = R"({"state":"%s","rssi":%d,"battery":%d})";
  static const char *devicePayloadNoBtt = R"({"state":"%s","rssi":%d})";

  void publishBLEState(const char address[ADDRESS_STRING_SIZE], const char state[4], int8_t rssi, int8_t batteryLevel)
  {
    const uint16_t maxTopicLen = strlen(getMQTTBaseSensorTopic()) + 22;
    char topic[maxTopicLen];
    char strbuff[5];

#if PUBLISH_SEPARATED_TOPICS
    snprintf(topic, maxTopicLen, "%s/%s/state", getMQTTBaseSensorTopic(), address);
    publishToMQTT(topic, state, false);
    snprintf(topic, maxTopicLen, "%s/%s/rssi", getMQTTBaseSensorTopic(), address);
    itoa(rssi, strbuff, 10);
    publishToMQTT(topic, strbuff, false);
#if PUBLISH_BATTERY_LEVEL
    snprintf(topic, maxTopicLen, "%s/%s/battery", getMQTTBaseSensorTopic(), address);
    itoa(batteryLevel, strbuff, 10);
    publishToMQTT(topic, strbuff, false);
#endif
#endif

#if PUBLISH_SIMPLE_JSON
    snprintf(topic, maxTopicLen, "%s/%s", getMQTTBaseSensorTopic(), address);
    const uint16_t maxPayloadLen = 45;
    char payload[maxPayloadLen];

#if PUBLISH_BATTERY_LEVEL
    snprintf(payload, maxPayloadLen, devicePayloadBtt, state, rssi, batteryLevel);
#else
    snprintf(payload, maxPayloadLen, devicePayloadNoBtt, state, rssi);
#endif
    publishToMQTT(topic, payload, false);
#endif
  }

  void publishSySInfo()
  {
    const size_t ssidlen = SettingsMngr.wifiSSID.length() + 1;
    const uint16_t maxSysPayloadLen = 83 + sizeof(VERSION) + ssidlen;
    char sysPayload[maxSysPayloadLen];
    static String IP;
    IP.reserve(16);
    IP = WiFi.localIP().toString();
    char strmilli[20];
    snprintf(sysPayload, maxSysPayloadLen, R"({"uptime":"%s","version":"%s","SSID":"%s","IP":"%s"})", formatMillis(millis(), strmilli), VERSION, SettingsMngr.wifiSSID.c_str(), IP.c_str());
    publishToMQTT(getMQTTSysInfoTopic(), sysPayload, false);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // HOME ASSISTANT DISCOVERY
  // https://www.home-assistant.io/docs/mqtt/discovery/

  // Define the common format string once as a static constant
  static const char *DISCOVERY_PAYLOAD_FORMAT =
      "{"
      "\"name\":\"%s\","
      "\"unique_id\":\"%s\","
      "\"state_topic\":\"%s\","
      "\"availability_topic\":\"%s\","
      "\"payload_available\":\"%s\","
      "\"payload_not_available\":\"%s\","
      "\"icon\":\"%s\","
      "\"device\":{"
      "\"identifiers\":[\"%s\"],"
      "\"name\":\"ESP32 BLETracker (%s)\","
      "\"model\":\"ESP32 BLETracker\","
      "\"manufacturer\":\"Shogunxam\","
      "\"sw_version\":\"%s\""
      "}";

  // Similarly for attribute formats
  static const char *SIMPLE_JSON_ATTR_FORMAT =
      ",\"value_template\":\"{%% if value_json.state == 'on' %%}home{%% else %%}not_home{%% endif %%}\","
      "\"json_attributes_topic\":\"%s\","
      "\"source_type\":\"bluetooth\"";

  static const char *SEPARATED_TOPICS_ATTR_FORMAT_WITH_BATTERY =
      ",\"json_attributes_topics\":[\"%s\", \"%s\"],"
      "\"source_type\":\"bluetooth\"";

  static const char *SEPARATED_TOPICS_ATTR_FORMAT_NO_BATTERY =
      ",\"json_attributes_topic\":\"%s\","
      "\"source_type\":\"bluetooth\"";

  static const char *TRACKER_STATUS_ATTR_FORMAT =
      ",\"value_template\":\"{{ value_json.status }}\"";

  static const char *DEVICES_LIST_ATTR_FORMAT =
      ",\"value_template\":\"{{ value_json.count }}\"";

  // Calculate required buffer size for discovery payload
  size_t calculateDiscoveryPayloadSize(const char *name, const char *uniqueId,
                                       const char *stateTopic, const char *icon,
                                       const char *gatewayName, const char *version)
  {
    // Calculate base size (format string length minus format specifiers)
    static const size_t baseSize = strlen(DISCOVERY_PAYLOAD_FORMAT) - 10 * 2; // 10 %s placeholders, each 2 chars

    // Add variable field lengths
    size_t dynamicSize = 0;
    dynamicSize += strlen(name);
    dynamicSize += strlen(uniqueId);
    dynamicSize += strlen(stateTopic);
    dynamicSize += strlen(getMQTTAvailabilityTopic());
    dynamicSize += strlen(MQTT_PAYLOAD_AVAILABLE);
    dynamicSize += strlen(MQTT_PAYLOAD_UNAVAILABLE);
    dynamicSize += strlen(icon);
    dynamicSize += strlen(gatewayName) * 2; // Gateway name appears twice
    dynamicSize += strlen(version);

    // Add some margin for safety
    return baseSize + dynamicSize + 20;
  }

  // Similarly for the specific attributes function
  size_t calculateSpecificAttributesSize(bool isSimpleJson, const char *deviceTopic,
                                         const char *rssiTopic = nullptr,
                                         const char *batteryTopic = nullptr)
  {
    // Calculate base size based on the format string being used
    size_t baseSize;
    size_t dynamicSize = 0;

    if (isSimpleJson)
    {
      baseSize = strlen(SIMPLE_JSON_ATTR_FORMAT) - 2; // One %s placeholder
      dynamicSize += strlen(deviceTopic);
    }
    else if (batteryTopic != nullptr)
    {
      baseSize = strlen(SEPARATED_TOPICS_ATTR_FORMAT_WITH_BATTERY) - 4; // Two %s placeholders
      dynamicSize += strlen(rssiTopic) + strlen(batteryTopic);
    }
    else
    {
      baseSize = strlen(SEPARATED_TOPICS_ATTR_FORMAT_NO_BATTERY) - 2; // One %s placeholder
      dynamicSize += strlen(rssiTopic);
    }

    // Add margin for safety
    return baseSize + dynamicSize + 20;
  }

  void generateCommonDiscoveryPayload(char *buffer, size_t bufferSize,
                                      const char *name, const char *uniqueId,
                                      const char *stateTopic, const char *icon = "mdi:bluetooth")
  {
    const char *availabilityTopic = getMQTTAvailabilityTopic();

    // Use the same format string defined above
    snprintf(buffer, bufferSize, DISCOVERY_PAYLOAD_FORMAT,
             name,
             uniqueId,
             stateTopic,
             availabilityTopic,
             MQTT_PAYLOAD_AVAILABLE,
             MQTT_PAYLOAD_UNAVAILABLE,
             icon,
             SettingsMngr.gateway.c_str(),
             SettingsMngr.gateway.c_str(),
             VERSION);
  }

  // Funzione per aggiungere attributi specifici al payload e chiudere il JSON
  void finalizeDiscoveryPayload(char *buffer, size_t bufferSize, size_t currentPos, const char *additionalAttributes = nullptr)
  {
    if (additionalAttributes && (currentPos + strlen(additionalAttributes) + 2 < bufferSize))
    {
      strncat(buffer + currentPos, additionalAttributes, bufferSize - currentPos - 2);
    }

    // Close the JSON if there's room
    size_t currentLen = strlen(buffer);
    if (currentLen + 1 < bufferSize)
    {
      strncat(buffer + currentLen, "}", bufferSize - currentLen - 1);
    }
  }

  bool publishTrackerDeviceDiscovery()
  {
    const char *bleTrackerStatusStr = "BLETracker Status";
    const char *audioIcoStr = "mdi:bluetooth-audio";

    char discoveryTopic[128];
    snprintf(discoveryTopic, sizeof(discoveryTopic),
             "homeassistant/sensor/%s_status/config",
             SettingsMngr.gateway.c_str());

    char stateTopic[64];
    snprintf(stateTopic, sizeof(stateTopic),
             "%s/status",
             getMQTTBaseSensorTopic());

    char uniqueId[32];
    snprintf(uniqueId, sizeof(uniqueId), "%s_status", SettingsMngr.gateway.c_str());

    // Calculate required buffer size
    size_t payloadSize = calculateDiscoveryPayloadSize(bleTrackerStatusStr, uniqueId,
                                                       stateTopic, audioIcoStr,
                                                       SettingsMngr.gateway.c_str(), VERSION);

    // Add size for specific attributes
    payloadSize += strlen(TRACKER_STATUS_ATTR_FORMAT) + 2; // +2 for closing brace and null terminator

    char discoveryPayload[payloadSize];
    generateCommonDiscoveryPayload(discoveryPayload, sizeof(discoveryPayload),
                                   bleTrackerStatusStr, uniqueId,
                                   stateTopic, audioIcoStr);

    // Add specific attributes and finalize
    finalizeDiscoveryPayload(discoveryPayload, payloadSize,
                             strlen(discoveryPayload), TRACKER_STATUS_ATTR_FORMAT);

    if (connectToMQTT())
    {
      _publishToMQTT(discoveryTopic, discoveryPayload, true);
      DEBUG_PRINTLN("INFO: Home Assistant discovery payload sent for BLE Tracker device");
      return true;
    }

    return false;
  }

  bool publishDevicesListSensorDiscovery()
  {
    const char *bleDevicesStr = "BLE Devices";
    char discoveryTopic[128];
    snprintf(discoveryTopic, sizeof(discoveryTopic),
             "homeassistant/sensor/%s_devices/config",
             SettingsMngr.gateway.c_str());

    char devicesTopic[64];
    snprintf(devicesTopic, sizeof(devicesTopic),
             "%s/devices",
             getMQTTBaseSensorTopic());

    char uniqueId[32];
    snprintf(uniqueId, sizeof(uniqueId), "%s_devices", SettingsMngr.gateway.c_str());

    // Calculate required buffer size
    size_t payloadSize = calculateDiscoveryPayloadSize(bleDevicesStr, uniqueId,
                                                       devicesTopic, "mdi:bluetooth",
                                                       SettingsMngr.gateway.c_str(), VERSION);

    // Add size for specific attributes
    payloadSize += strlen(DEVICES_LIST_ATTR_FORMAT) + 2; // +2 for closing brace and null terminator

    char discoveryPayload[payloadSize];
    generateCommonDiscoveryPayload(discoveryPayload, sizeof(discoveryPayload),
                                   bleDevicesStr, uniqueId, devicesTopic);

    // Add specific attributes and finalize
    finalizeDiscoveryPayload(discoveryPayload, payloadSize,
                             strlen(discoveryPayload), DEVICES_LIST_ATTR_FORMAT);

    if (connectToMQTT())
    {
      _publishToMQTT(discoveryTopic, discoveryPayload, true);
      DEBUG_PRINTLN("INFO: Home Assistant discovery payload sent for BLE Devices list sensor");
      return true;
    }

    return false;
  }

  bool publishBLEDeviceSensorDiscovery(const BLETrackedDevice &device)
  {
    char discoveryTopic[128];
    snprintf(discoveryTopic, sizeof(discoveryTopic),
             "homeassistant/device_tracker/%s_device_%s/config",
             SettingsMngr.gateway.c_str(), device.address);

    char deviceName[64];
    snprintf(deviceName, sizeof(deviceName), "BLE Device %s", device.address);

    char uniqueId[48];
    snprintf(uniqueId, sizeof(uniqueId), "%s_device_%s", SettingsMngr.gateway.c_str(), device.address);

    char deviceTopic[64];
#if PUBLISH_SIMPLE_JSON
    snprintf(deviceTopic, sizeof(deviceTopic),
             "%s/%s",
             getMQTTBaseSensorTopic(), device.address);
#elif PUBLISH_SEPARATED_TOPICS
    snprintf(deviceTopic, sizeof(deviceTopic),
             "%s/%s/state",
             getMQTTBaseSensorTopic(), device.address);
#endif

    // Calculate required buffer size
    size_t payloadSize = calculateDiscoveryPayloadSize(deviceName, uniqueId, deviceTopic,
                                                       "mdi:bluetooth", SettingsMngr.gateway.c_str(), VERSION);

    // Prepare for specific attributes calculation
    char rssiTopic[64] = {0};
    char batteryTopic[64] = {0};

#if PUBLISH_SEPARATED_TOPICS
    snprintf(rssiTopic, sizeof(rssiTopic),
             "%s/%s/rssi",
             getMQTTBaseSensorTopic(), device.address);
#if PUBLISH_BATTERY_LEVEL
    snprintf(batteryTopic, sizeof(batteryTopic),
             "%s/%s/battery",
             getMQTTBaseSensorTopic(), device.address);
#endif
#endif

    // Calculate size for specific attributes
    const char *batteryTopicPtr = (PUBLISH_BATTERY_LEVEL && PUBLISH_SEPARATED_TOPICS) ? batteryTopic : nullptr;
    size_t attrSize = calculateSpecificAttributesSize(PUBLISH_SIMPLE_JSON, deviceTopic, rssiTopic, batteryTopicPtr);

    // Add attribute size to payload size
    payloadSize += attrSize;

    char discoveryPayload[payloadSize];
    generateCommonDiscoveryPayload(discoveryPayload, sizeof(discoveryPayload),
                                   deviceName, uniqueId, deviceTopic);

    // Generate specific attributes based on configuration
    char specificAttributes[attrSize];
#if PUBLISH_SIMPLE_JSON
    snprintf(specificAttributes, attrSize, SIMPLE_JSON_ATTR_FORMAT, deviceTopic);
#elif PUBLISH_SEPARATED_TOPICS
#if PUBLISH_BATTERY_LEVEL
    snprintf(specificAttributes, attrSize, SEPARATED_TOPICS_ATTR_FORMAT_WITH_BATTERY,
             rssiTopic, batteryTopic);
#else
    snprintf(specificAttributes, attrSize, SEPARATED_TOPICS_ATTR_FORMAT_NO_BATTERY, rssiTopic);
#endif
#endif

    // Finalize the payload
    finalizeDiscoveryPayload(discoveryPayload, payloadSize,
                             strlen(discoveryPayload), specificAttributes);

    // Pubblica il discovery
    if (connectToMQTT())
    {
      _publishToMQTT(discoveryTopic, discoveryPayload, true);
      DEBUG_PRINTF("INFO: Home Assistant discovery payload sent for BLE Device %s sensor\n", device.address);
      auto& BLETrackedDevices = BLEScanTask::GetTrackedDeviceList();
      BLETrackedDevices.ExcuteFunctionOnDevice(device.address, [](BLETrackedDevice &trackedDevice)
        {
          trackedDevice.haDiscoveryPublished = true;
        });

      return true;
    }

    return false;
  }

  void publishTrackerStatus()
  {
    char stateTopic[64];
    snprintf(stateTopic, sizeof(stateTopic),
             "%s/status",
             getMQTTBaseSensorTopic());

    char statusPayload[128];
    char strmilli[20];
    snprintf(statusPayload, sizeof(statusPayload),
             "{"
             "\"status\":\"online\","
             "\"uptime\":\"%s\","
             "\"ip\":\"%s\","
             "\"devices_count\":%zu"
             "}",
             formatMillis(millis(), strmilli),
             WiFi.localIP().toString().c_str(),
             BLEScanTask::GetTrackedDevicesCount());

    publishToMQTT(stateTopic, statusPayload, false);
  }

  void publishDevicesList()
  {
    // Topic for the list of devices
    char devicesTopic[64];
    snprintf(devicesTopic, sizeof(devicesTopic),
             "%s/devices",
             getMQTTBaseSensorTopic());

    std::vector<BLETrackedDevice> trackedDevices;
    BLEScanTask::GetTrackedDevices(trackedDevices);
    // Create a buffer for the JSON payload
    const size_t maxPayloadSize = 128 + (trackedDevices.size() * 50);
    char *payload = new char[maxPayloadSize];

    // Start the JSON payload
    int written = snprintf(payload, maxPayloadSize,
                           "{"
                           "\"count\":%zu,"
                           "\"devices\":[",
                           trackedDevices.size());

    // Add each device to the JSON
    bool firstDevice = true;
    for (const auto &device : trackedDevices)
    {
      if (!firstDevice)
      {
        written += snprintf(payload + written, maxPayloadSize - written, ",");
      }

      const char *state = device.isDiscovered ? MQTT_PAYLOAD_ON : MQTT_PAYLOAD_OFF;

      written += snprintf(payload + written, maxPayloadSize - written,
                          "{"
                          "\"address\":\"%s\","
                          "\"state\":\"%s\","
                          "\"rssi\":%d,"
                          "\"last_seen\":%ld"
                          "}",
                          device.address, state, device.rssiValue, device.lastDiscoveryTime);

      firstDevice = false;
    }

    // Close the JSON
    written += snprintf(payload + written, maxPayloadSize - written, "]}");

    // Pubblish the list of devices
    publishToMQTT(devicesTopic, payload, false);

    delete[] payload;
  }
  ////////////////////////////////////////////////////////////////////////////////

  void publishBLEState(const BLETrackedDevice &device)
  {
#if ENABLE_HOME_ASSISTANT_MQTT_DISCOVERY
    // Verify if it's necessary to do the discovery for this device
    if (!device.haDiscoveryPublished)
    {
      publishBLEDeviceSensorDiscovery(device);
    }

    // Update and publish the list of devices and tracker status
    static unsigned long lastDevicesListUpdate = 0;
    unsigned long now = millis();

    if (now - lastDevicesListUpdate > 30000)
    { // Ogni 30 secondi
      publishDevicesList();
      publishTrackerStatus();
      lastDevicesListUpdate = now;
    }
#endif // ENABLE_HOME_ASSISTANT_MQTT_DISCOVERY

    // Extabilish the state of the device
    const char *state = device.isDiscovered ? MQTT_PAYLOAD_ON : MQTT_PAYLOAD_OFF;
    int rssi = device.isDiscovered ? device.rssiValue : -100;

    // Use the existing code to publish device data
#if PUBLISH_SEPARATED_TOPICS
    const uint16_t maxTopicLen = strlen(getMQTTBaseSensorTopic()) + 22;
    char topic[maxTopicLen];
    char strbuff[5];

    snprintf(topic, maxTopicLen, "%s/%s/state", getMQTTBaseSensorTopic(), device.address);
    publishToMQTT(topic, state, false);

    snprintf(topic, maxTopicLen, "%s/%s/rssi", getMQTTBaseSensorTopic(), device.address);
    itoa(rssi, strbuff, 10);
    publishToMQTT(topic, strbuff, false);

#if PUBLISH_BATTERY_LEVEL
    snprintf(topic, maxTopicLen, "%s/%s/battery", getMQTTBaseSensorTopic(), device.address);
    itoa(device.batteryLevel, strbuff, 10);
    publishToMQTT(topic, strbuff, false);
#endif
#endif

#if PUBLISH_SIMPLE_JSON
    const uint16_t maxTopicLen = strlen(getMQTTBaseSensorTopic()) + 22;
    char topic[maxTopicLen];
    snprintf(topic, maxTopicLen, "%s/%s", getMQTTBaseSensorTopic(), device.address);

    const uint16_t maxPayloadLen = 45;
    char payload[maxPayloadLen];

#if PUBLISH_BATTERY_LEVEL
    snprintf(payload, maxPayloadLen, devicePayloadBtt,
             state, rssi, device.batteryLevel);
#else
    snprintf(payload, maxPayloadLen, devicePayloadNoBtt,
             state, rssi);
#endif
    publishToMQTT(topic, payload, false);
#endif
  }

  void mqttLoop()
  {
    mqttClient.loop();
  }
}
#endif /*USE_MQTT*/