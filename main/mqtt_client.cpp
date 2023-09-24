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

const char* getMQTTBaseSensorTopic()
{
  static char MQTT_BASE_SENSOR_TOPIC[50] = "";
  if( MQTT_BASE_SENSOR_TOPIC[0] == 0)
  {
    snprintf(MQTT_BASE_SENSOR_TOPIC,50,"%s/%s", LOCATION, SettingsMngr.gateway);
  }

  return MQTT_BASE_SENSOR_TOPIC;
}

const char* getMQTTAvailabilityTopic()
{
  static char MQTT_AVAILABILITY_TOPIC[60] = "";
  if(MQTT_AVAILABILITY_TOPIC[0] == 0)
  {
     snprintf(MQTT_AVAILABILITY_TOPIC,60,"%s/LWT", getMQTTBaseSensorTopic());
  }
  return MQTT_AVAILABILITY_TOPIC;
}

const char* getMQTTSysInfoTopic()
{
  static char MQTT_SYSINFO_TOPIC[60] = "";
  if(MQTT_SYSINFO_TOPIC[0] == 0)
  {
     snprintf(MQTT_SYSINFO_TOPIC,60,"%s/sysinfo", getMQTTBaseSensorTopic());
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

void initializeMQTT()
{
    mqttClient.setServer(SettingsMngr.mqttServer.c_str(), SettingsMngr.mqttPort);
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

void mqttLoop()
{
    mqttClient.loop();
}
#endif /*USE_MQTT*/