#ifndef _MQTT_CLIENT_H__
#define _MQTT_CLIENT_H__
#include "main.h"
namespace MQTTClient
{
    void initializeMQTT();
    bool connectToMQTT();
    void mqttLoop();
    void publishBLEState(const char address[ADDRESS_STRING_SIZE], const char state[4], int8_t rssi, int8_t batteryLevel);
    bool publishHomeAssistantDiscovery(const char address[ADDRESS_STRING_SIZE]);
    void publishToMQTT(const char *topic, const char *payload, bool retain);
    void publishAvailabilityToMQTT();
    void publishSySInfo();

    ///////////////////////////////////////////////////////////////
    // Home Assistant Discovery
    void publishBLEState(const BLETrackedDevice &device);
    void publishDevicesList();
    void publishTrackerStatus();
    bool publishBLEDeviceSensorDiscovery(const BLETrackedDevice &device);
    bool publishDevicesListSensorDiscovery();
    bool publishTrackerDeviceDiscovery();
    ///////////////////////////////////////////////////////////////
}
#endif /*_MQTT_CLIENT_H__*/