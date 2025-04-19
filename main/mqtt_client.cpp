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

    publishTrackerStatus();
    publishDevicesList();
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
// Funzione per pubblicare il discovery del dispositivo principale (ESP32 BLE Tracker)
bool publishTrackerDeviceDiscovery()
{
  // Topic per il discovery del dispositivo principale
  char discoveryTopic[128];
  snprintf(discoveryTopic, sizeof(discoveryTopic), 
          "homeassistant/sensor/%s_status/config", 
          SettingsMngr.gateway);
  
  // Topic per lo stato del tracker
  char stateTopic[64];
  snprintf(stateTopic, sizeof(stateTopic), 
          "%s/status", 
          getMQTTBaseSensorTopic());
  
  // Payload di discovery per il sensore di stato
  char discoveryPayload[384];
  snprintf(discoveryPayload, sizeof(discoveryPayload), 
          "{"
          "\"name\":\"BLE Tracker Status\","
          "\"unique_id\":\"%s_status\","
          "\"state_topic\":\"%s\","
          "\"value_template\":\"{{ value_json.status }}\","
          "\"icon\":\"mdi:bluetooth-scanner\","
          "\"device\":{"
            "\"identifiers\":[\"%s\"],"
            "\"name\":\"ESP32 BLETracker (%s)\","
            "\"model\":\"ESP32\","
            "\"manufacturer\":\"Espressif\","
            "\"sw_version\":\"%s\""
          "}"
          "}",
          SettingsMngr.gateway,
          stateTopic,
          SettingsMngr.gateway,
          SettingsMngr.gateway,
          VERSION);
  
  // Pubblica il discovery
  if (connectToMQTT()) {
    _publishToMQTT(discoveryTopic, discoveryPayload, true);
    DEBUG_PRINTLN("INFO: Home Assistant discovery payload sent for BLE Tracker device");
    return true;
  }
  
  return false;
}

// Funzione per pubblicare il discovery del sensore della lista dispositivi
bool publishDevicesListSensorDiscovery()
{
  // Topic per il discovery del sensore della lista dispositivi
  char discoveryTopic[128];
  snprintf(discoveryTopic, sizeof(discoveryTopic), 
          "homeassistant/sensor/%s_devices/config", 
          SettingsMngr.gateway);
  
  // Topic per la lista dei dispositivi
  char devicesTopic[64];
  snprintf(devicesTopic, sizeof(devicesTopic), 
          "%s/devices", 
          getMQTTBaseSensorTopic());
  
  // Payload di discovery per il sensore della lista dispositivi
  char discoveryPayload[384];
  snprintf(discoveryPayload, sizeof(discoveryPayload), 
          "{"
          "\"name\":\"BLE Devices\","
          "\"unique_id\":\"%s_devices\","
          "\"state_topic\":\"%s\","
          "\"value_template\":\"{{ value_json.count }}\","
          "\"icon\":\"mdi:bluetooth\","
          "\"device\":{"
            "\"identifiers\":[\"%s\"],"
            "\"name\":\"ESP32 BLE Tracker (%s)\","
            "\"model\":\"ESP32\","
            "\"manufacturer\":\"Espressif\","
            "\"sw_version\":\"%s\""
          "}"
          "}",
          SettingsMngr.gateway,
          devicesTopic,
          SettingsMngr.gateway,
          SettingsMngr.gateway,
          VERSION);
  
  // Pubblica il discovery
  if (connectToMQTT()) {
    _publishToMQTT(discoveryTopic, discoveryPayload, true);
    DEBUG_PRINTLN("INFO: Home Assistant discovery payload sent for BLE Devices list sensor");
    return true;
  }
  
  return false;
}

// Funzione per pubblicare il discovery di un singolo dispositivo BLE
bool publishBLEDeviceSensorDiscovery(const BLETrackedDevice& device)
{
  // Topic per il discovery del sensore del dispositivo BLE
  char discoveryTopic[128];
  snprintf(discoveryTopic, sizeof(discoveryTopic), 
          "homeassistant/device_tracker/%s_device_%s/config", 
          SettingsMngr.gateway, device.address);
  
  // Nome del dispositivo
  char deviceName[64];
  snprintf(deviceName, sizeof(deviceName), "BLE Device %s", device.address);
  
  // Topic per i dati del dispositivo
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
  
  // Payload di discovery per il sensore del dispositivo BLE
  char discoveryPayload[512];
  
#if PUBLISH_SIMPLE_JSON
  snprintf(discoveryPayload, sizeof(discoveryPayload), 
          "{"
          "\"name\":\"%s\","
          "\"unique_id\":\"%s_device_%s\","
          "\"state_topic\":\"%s\","
          "\"value_template\":\"{%% if value_json.state == 'on' %%}home{%% else %%}not_home{%% endif %%}\","
          "\"json_attributes_topic\":\"%s\","
          "\"icon\":\"mdi:bluetooth\","
          "\"device\":{"
            "\"identifiers\":[\"%s\"],"
            "\"name\":\"ESP32 BLE Tracker (%s)\","
            "\"model\":\"ESP32\","
            "\"manufacturer\":\"Espressif\","
            "\"sw_version\":\"%s\""
          "}"
          "}",
          deviceName,
          SettingsMngr.gateway, device.address,
          deviceTopic,
          deviceTopic,
          SettingsMngr.gateway,
          SettingsMngr.gateway,
          VERSION);
#elif PUBLISH_SEPARATED_TOPICS
  // Per topic separati, usa payload_home e payload_not_home
  // e configura json_attributes_topic per il topic RSSI
  char rssiTopic[64];
  snprintf(rssiTopic, sizeof(rssiTopic), 
          "%s/%s/rssi", 
          getMQTTBaseSensorTopic(), device.address);
          
#if PUBLISH_BATTERY_LEVEL
  char batteryTopic[64];
  snprintf(batteryTopic, sizeof(batteryTopic), 
          "%s/%s/battery", 
          getMQTTBaseSensorTopic(), device.address);
          
  snprintf(discoveryPayload, sizeof(discoveryPayload), 
          "{"
          "\"name\":\"%s\","
          "\"unique_id\":\"%s_device_%s\","
          "\"state_topic\":\"%s\","
          "\"icon\":\"mdi:bluetooth\","
          "\"json_attributes_topics\":[\"%s\", \"%s\"],"
          "\"device\":{"
            "\"identifiers\":[\"%s\"],"
            "\"name\":\"ESP32 BLE Tracker (%s)\","
            "\"model\":\"ESP32\","
            "\"manufacturer\":\"Espressif\","
            "\"sw_version\":\"%s\""
          "}"
          "}",
          deviceName,
          SettingsMngr.gateway, device.address,
          deviceTopic,
          rssiTopic, batteryTopic,
          SettingsMngr.gateway,
          SettingsMngr.gateway,
          VERSION);
#else
  snprintf(discoveryPayload, sizeof(discoveryPayload), 
          "{"
          "\"name\":\"%s\","
          "\"unique_id\":\"%s_device_%s\","
          "\"state_topic\":\"%s\","
          "\"icon\":\"mdi:bluetooth\","
          "\"json_attributes_topic\":\"%s\","
          "\"device\":{"
            "\"identifiers\":[\"%s\"],"
            "\"name\":\"ESP32 BLE Tracker (%s)\","
            "\"model\":\"ESP32\","
            "\"manufacturer\":\"Espressif\","
            "\"sw_version\":\"%s\""
          "}"
          "}",
          deviceName,
          SettingsMngr.gateway, device.address,
          deviceTopic,
          rssiTopic,
          SettingsMngr.gateway,
          SettingsMngr.gateway,
          VERSION);
#endif
#endif
  
  // Pubblica il discovery
  if (connectToMQTT()) {
    _publishToMQTT(discoveryTopic, discoveryPayload, true);
    DEBUG_PRINTF("INFO: Home Assistant discovery payload sent for BLE Device %s sensor\n", device.address);
    
    // Aggiorna il flag haDiscoveryPublished
    // Nota: dobbiamo trovare il dispositivo nella lista globale perché abbiamo una copia costante
    for (auto& trackedDevice : BLETrackedDevices) {
      if (strcmp(trackedDevice.address, device.address) == 0) {
        trackedDevice.haDiscoveryPublished = true;
        break;
      }
    }
    
    return true;
  }
  
  return false;
}

// Funzione per pubblicare lo stato del tracker
void publishTrackerStatus()
{
  // Topic per lo stato del tracker
  char stateTopic[64];
  snprintf(stateTopic, sizeof(stateTopic), 
          "%s/status", 
          getMQTTBaseSensorTopic());
  
  // Payload con informazioni sul tracker
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
  
  // Pubblica lo stato
  publishToMQTT(stateTopic, statusPayload, false);
}

// Funzione per pubblicare la lista dei dispositivi
void publishDevicesList()
{
  // Topic per la lista dei dispositivi
  char devicesTopic[64];
  snprintf(devicesTopic, sizeof(devicesTopic), 
          "%s/devices", 
          getMQTTBaseSensorTopic());
  
  // Crea un buffer per il payload JSON
  const size_t maxPayloadSize = 128 + (BLETrackedDevices.size() * 50);
  char* payload = new char[maxPayloadSize];
  
  // Inizia il JSON
  int written = snprintf(payload, maxPayloadSize, 
          "{"
          "\"count\":%zu,"
          "\"devices\":[", 
          BLETrackedDevices.size());
  
  // Aggiungi ogni dispositivo
  bool firstDevice = true;
  for (const auto& device : BLETrackedDevices) {
    if (!firstDevice) {
      written += snprintf(payload + written, maxPayloadSize - written, ",");
    }
    
    const char* state = device.isDiscovered ? MQTT_PAYLOAD_ON : MQTT_PAYLOAD_OFF;
    
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
  
  // Chiudi il JSON
  written += snprintf(payload + written, maxPayloadSize - written, "]}");
  
  // Pubblica la lista
  publishToMQTT(devicesTopic, payload, false);
  
  // Libera la memoria
  delete[] payload;
}

// Funzione per pubblicare lo stato di un dispositivo BLE (versione che accetta un oggetto BLETrackedDevice)
void publishBLEState(const BLETrackedDevice& device)
{
  // Verifica se è necessario fare il discovery per questo dispositivo
  if (!device.haDiscoveryPublished) {
    publishBLEDeviceSensorDiscovery(device);
  }
  
  // Aggiorna e pubblica la lista dei dispositivi e lo stato del tracker
  static unsigned long lastDevicesListUpdate = 0;
  unsigned long now = millis();
  
  if (now - lastDevicesListUpdate > 30000) { // Ogni 30 secondi
    publishDevicesList();
    publishTrackerStatus();
    lastDevicesListUpdate = now;
  }
  
  // Determina lo stato (on/off) in base al campo isDiscovered
  const char* state = device.isDiscovered ? MQTT_PAYLOAD_ON : MQTT_PAYLOAD_OFF;
  int rssi = device.isDiscovered ? device.rssiValue : -100;

  // Usa il codice esistente per pubblicare i dati del dispositivo
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

////////////////////////////////////////////////////////////////////////////////

void mqttLoop()
{
    mqttClient.loop();
}
#endif /*USE_MQTT*/