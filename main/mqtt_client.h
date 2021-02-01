#ifndef _MQTT_CLIENT_H__
#define _MQTT_CLIENT_H__
#include "main.h"

void initializeMQTT();
bool connectToMQTT();
void mqttLoop();
void publishBLEState(const char address[ADDRESS_STRING_SIZE], const char state[4], int8_t rssi, int8_t batteryLevel);
void publishToMQTT(const char *topic, const char *payload, bool retain);
void publishAvailabilityToMQTT();
void publishSySInfo();

#endif /*_MQTT_CLIENT_H__*/