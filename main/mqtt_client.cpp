#include "config.h"
#if USE_MQTT
#include "mqtt_client.h"
#include "DebugPrint.h"
#include "SPIFFSLogger.h"
#include "WiFiManager.h"
#include "watchdog.h"
#include "settings.h"

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

PubSubClient mqttClient(wifiClient);

///////////////////////////////////////////////////////////////////////////
//   MQTT
///////////////////////////////////////////////////////////////////////////
volatile unsigned long lastMQTTConnection = 0;
/*
  Function called to connect/reconnect to the MQTT broker
*/
static bool firstTimeMQTTConnection = true;
static bool MQTTConnectionErrorSignaled = false;
static uint32_t MQTTErrorCounter = 0;

const char *getMQTTBaseSensorTopic()
{
  static char MQTT_BASE_SENSOR_TOPIC[50] = "";
  if (MQTT_BASE_SENSOR_TOPIC[0] == 0)
  {
    snprintf(MQTT_BASE_SENSOR_TOPIC, 50, "%s/%s", LOCATION, SettingsMngr.gateway);
  }

  return MQTT_BASE_SENSOR_TOPIC;
}

const char *getMQTTAvailabilityTopic()
{
  static char MQTT_AVAILABILITY_TOPIC[60] = "";
  if (MQTT_AVAILABILITY_TOPIC[0] == 0)
  {
    snprintf(MQTT_AVAILABILITY_TOPIC, 60, "%s/LWT", getMQTTBaseSensorTopic());
  }
  return MQTT_AVAILABILITY_TOPIC;
}

const char *getMQTTSysInfoTopic()
{
  static char MQTT_SYSINFO_TOPIC[60] = "";
  if (MQTT_SYSINFO_TOPIC[0] == 0)
  {
    snprintf(MQTT_SYSINFO_TOPIC, 60, "%s/sysinfo", getMQTTBaseSensorTopic());
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
  for (;;)
  {
    if (mqttClient.connected())
    {
      mqttClient.loop();
    }
    else
    {
      // Tentativo di riconnessione se disconnesso
      if (WiFi.status() == WL_CONNECTED)
      {
        DEBUG_PRINTLN("MQTT disconnected, attempting reconnection...");
        connectToMQTT();
      }
    }
    delay(100); // Esegui il loop MQTT 10 volte al secondo
  }
}

void initializeMQTT()
{
  mqttClient.setServer(SettingsMngr.mqttServer.c_str(), SettingsMngr.mqttPort);
  xTaskCreatePinnedToCore(
      MQTTLoopTask,
      "MQTTLoopTask",
      4096,
      NULL,
      1,
      NULL,
      0);
}

bool connectToMQTT()
{
  WiFiConnect(SettingsMngr.wifiSSID, SettingsMngr.wifiPwd);

  uint8_t maxRetry = 3;
  while (!mqttClient.connected())
  {
    DEBUG_PRINTF("INFO: Connecting to MQTT broker: %s\n", SettingsMngr.mqttServer.c_str());
    if (!firstTimeMQTTConnection && !MQTTConnectionErrorSignaled)
      LOG_TO_FILE_E("Error: MQTT broker disconnected, connecting...");
    DEBUG_PRINTLN(F("Error: MQTT broker disconnected, connecting..."));
    if (mqttClient.connect(GATEWAY_NAME, SettingsMngr.mqttUser.c_str(), SettingsMngr.mqttPwd.c_str(), getMQTTAvailabilityTopic(), 1, true, MQTT_PAYLOAD_UNAVAILABLE))
    {
      DEBUG_PRINTLN(F("INFO: The client is successfully connected to the MQTT broker"));
      LOG_TO_FILE_I("MQTT broker connected!");
      MQTTConnectionErrorSignaled = false;
      _publishToMQTT(getMQTTAvailabilityTopic(), MQTT_PAYLOAD_AVAILABLE, true);
    }
    else
    {
      DEBUG_PRINTLN(F("ERROR: The connection to the MQTT broker failed"));
      DEBUG_PRINTF("INFO: MQTT username: %s\n", SettingsMngr.mqttUser.c_str())
      DEBUG_PRINTF("INFO: MQTT password: %s\n", SettingsMngr.mqttPwd.c_str());
      DEBUG_PRINTF("INFO: MQTT broker: %s\n", SettingsMngr.mqttServer.c_str());
#if ENABLE_FILE_LOG
      uint32_t numLogs = SPIFFSLogger.numOfLogsPerSession();
      uint32_t deltaLogs;

      if (numLogs < MQTTErrorCounter)
        deltaLogs = ~uint32_t(0) - MQTTErrorCounter + numLogs;
      else
        deltaLogs = numLogs - MQTTErrorCounter;

      if (deltaLogs >= 500)
        MQTTConnectionErrorSignaled = false;

      if (!MQTTConnectionErrorSignaled)
      {
        LOG_TO_FILE_E("Error: Connection to the MQTT broker failed!");
        MQTTConnectionErrorSignaled = true;
        MQTTErrorCounter = SPIFFSLogger.numOfLogsPerSession();
      }
#endif
    }

    Watchdog::Feed();

    maxRetry--;
    if (maxRetry == 0)
      return false;
  }
  firstTimeMQTTConnection = false;
  return true;
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
  snprintf(payload, maxPayloadLen, R"({"state":"%s","rssi":%d,"battery":%d})", state, rssi, batteryLevel);
#if PUBLISH_BATTERY_LEVEL
  snprintf(payload, maxPayloadLen, R"({"state":"%s","rssi":%d,"battery":%d})", state, rssi, batteryLevel);
#else
  snprintf(payload, maxPayloadLen, R"({"state":"%s","rssi":%d})", state, rssi);
#endif
  publishToMQTT(topic, payload, false);
#endif
}

void publishSySInfo()
{
  constexpr uint16_t maxSysPayloadLen = 83 + sizeof(VERSION) + sizeof(WIFI_SSID);
  char sysPayload[maxSysPayloadLen];
  static String IP;
  IP.reserve(16);
  IP = WiFi.localIP().toString();
  char strmilli[20];
  snprintf(sysPayload, maxSysPayloadLen, R"({"uptime":"%s","version":"%s","SSID":"%s","IP":"%s"})", formatMillis(millis(), strmilli), VERSION, WIFI_SSID, IP.c_str());
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
           SettingsMngr.gateway,
           SettingsMngr.gateway,
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
  char discoveryTopic[128];
  snprintf(discoveryTopic, sizeof(discoveryTopic),
           "homeassistant/sensor/%s_status/config",
           SettingsMngr.gateway);

  char stateTopic[64];
  snprintf(stateTopic, sizeof(stateTopic),
           "%s/status",
           getMQTTBaseSensorTopic());

  char uniqueId[32];
  snprintf(uniqueId, sizeof(uniqueId), "%s_status", SettingsMngr.gateway);

  // Calculate required buffer size
  size_t payloadSize = calculateDiscoveryPayloadSize("BLETracker Status", uniqueId,
                                                     stateTopic, "mdi:bluetooth-audio",
                                                     SettingsMngr.gateway.c_str(), VERSION);

  // Add size for specific attributes
  payloadSize += strlen(TRACKER_STATUS_ATTR_FORMAT) + 2; // +2 for closing brace and null terminator

  char discoveryPayload[payloadSize];
  generateCommonDiscoveryPayload(discoveryPayload, sizeof(discoveryPayload),
                                 "BLETracker Status", uniqueId,
                                 stateTopic, "mdi:bluetooth-audio");

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
  char discoveryTopic[128];
  snprintf(discoveryTopic, sizeof(discoveryTopic),
           "homeassistant/sensor/%s_devices/config",
           SettingsMngr.gateway);

  char devicesTopic[64];
  snprintf(devicesTopic, sizeof(devicesTopic),
           "%s/devices",
           getMQTTBaseSensorTopic());

  char uniqueId[32];
  snprintf(uniqueId, sizeof(uniqueId), "%s_devices", SettingsMngr.gateway);

  // Calculate required buffer size
  size_t payloadSize = calculateDiscoveryPayloadSize("BLE Devices", uniqueId,
                                                     devicesTopic, "mdi:bluetooth",
                                                     SettingsMngr.gateway.c_str(), VERSION);

  // Add size for specific attributes
  payloadSize += strlen(DEVICES_LIST_ATTR_FORMAT) + 2; // +2 for closing brace and null terminator

  char discoveryPayload[payloadSize];
  generateCommonDiscoveryPayload(discoveryPayload, sizeof(discoveryPayload),
                                 "BLE Devices", uniqueId, devicesTopic);

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
           SettingsMngr.gateway, device.address);

  char deviceName[64];
  snprintf(deviceName, sizeof(deviceName), "BLE Device %s", device.address);

  char uniqueId[48];
  snprintf(uniqueId, sizeof(uniqueId), "%s_device_%s", SettingsMngr.gateway, device.address);

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

    for (auto &trackedDevice : BLETrackedDevices)
    {
      if (strcmp(trackedDevice.address, device.address) == 0)
      {
        trackedDevice.haDiscoveryPublished = true;
        break;
      }
    }

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
           BLETrackedDevices.size());

  publishToMQTT(stateTopic, statusPayload, false);
}

void publishDevicesList()
{
  // Topic for the list of devices
  char devicesTopic[64];
  snprintf(devicesTopic, sizeof(devicesTopic),
           "%s/devices",
           getMQTTBaseSensorTopic());

  // Create a buffer for the JSON payload
  const size_t maxPayloadSize = 128 + (BLETrackedDevices.size() * 50);
  char *payload = new char[maxPayloadSize];

  // Start the JSON payload
  int written = snprintf(payload, maxPayloadSize,
                         "{"
                         "\"count\":%zu,"
                         "\"devices\":[",
                         BLETrackedDevices.size());

  // Add each device to the JSON
  bool firstDevice = true;
  for (const auto &device : BLETrackedDevices)
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
  snprintf(payload, maxPayloadLen, R"({"state":"%s","rssi":%d,"battery":%d})",
           state, rssi, device.batteryLevel);
#else
  snprintf(payload, maxPayloadLen, R"({"state":"%s","rssi":%d})",
           state, rssi);
#endif
  publishToMQTT(topic, payload, false);
#endif
}

void mqttLoop()
{
  mqttClient.loop();
}

#endif /*USE_MQTT*/