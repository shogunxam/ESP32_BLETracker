#include "config.h"
#if USE_MESH
#include <painlessMesh.h>
#include "main.h"
#include "config.h"
#include "DebugPrint.h"
#include "mqtt_client.h"
#include "NetworkSync.h"
#include "Scheduler.h"

namespace meshNetwork
{
    // Callback methods prototypes
    void publishNodeId();
    void meshUpdate();

    static painlessMesh mesh;
    static uint32_t bridgeNodeId = 0;

    void receivedCallback(const uint32_t &from, const String &msg)
    {
        DEBUG_PRINTF("meshMsg from %d - %s", from, msg);
        std::istringstream iss(msg.c_str());
#if MESH_BRIDGE_NODE
        std::string address;
        std::string state;
        int rssi;
        int battery;
        while (!iss.eof())
        {
            iss >> address >> state >> rssi >> battery;
            DEBUG_PRINTF("received %s %s,%d,%d from %d", address.c_str(), state.c_str(), rssi, battery, from);
            publishBLEState(address.c_str(), state.c_str(), int8_t(rssi), int8_t(battery), from);
        }
#else
        iss >> bridgeNodeId;
#endif
    }

    void newConnectionCallback(uint32_t nodeId)
    {
        Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
    }

    void changedConnectionCallback()
    {
        Serial.printf("Changed connections\n");
    }

    uint32_t getNodeId()
    {
        return mesh.getNodeId();
    }

    void publishNodeId()
    {
        DEBUG_PRINTLN("executing publishNodeId");
        NetworkAcquire();
        mesh.sendBroadcast(String(mesh.getNodeId()));
        NetworkRelease();
    }

    void publish()
    {
        if (bridgeNodeId != 0)
        {
            std::ostringstream oss;
            for (auto &trackedDevice : BLETrackedDevices)
            {
                oss << trackedDevice.address;
                if (trackedDevice.isDiscovered)
                    oss << " " << MQTT_PAYLOAD_ON << " " << trackedDevice.rssiValue;
                else
                    oss << " " << MQTT_PAYLOAD_OFF << " " << -100;
                oss << " " << trackedDevice.batteryLevel << std::endl;
            }
            std::string msg = oss.str();
            DEBUG_PRINTF("publishing: %s ", msg.c_str());
            NetworkAcquire();
            mesh.sendSingle(bridgeNodeId, msg.c_str());
            NetworkRelease();
        }
    }

    void setup()
    {
        mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION); // set before init() so that you can see startup messages

        mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, 11);
        
#if MESH_BRIDGE_NODE
        // Channel set to 6. Make sure to use the same channel for your mesh and for you other
        // network (STATION_SSID)
        mesh.stationManual(WIFI_SSID, WIFI_PASSWORD);
        mesh.setHostname(GATEWAY_NAME);

        // Bridge node, should (in most cases) be a root node. See [the wiki](https://gitlab.com/painlessMesh/painlessMesh/wikis/Possible-challenges-in-mesh-formation) for some background
        mesh.setRoot(true);
#endif
        mesh.onReceive(&receivedCallback);
        // This node and all other nodes should ideally know the mesh contains a root, so call this on all nodes
        mesh.setContainsRoot(true);
    }

    bool hasIP()
    {
        DEBUG_PRINTLN("Station IP is " + mesh.getStationIP().toString());
        return mesh.getStationIP() != IPAddress(uint32_t(0));
    }

    void meshUpdate()
    {
        DEBUG_PRINTLN("executing meshUpdate");
        NetworkAcquire();
        mesh.update();
        NetworkRelease();
    }

    void loop()
    {
        DEBUG_PRINTLN("executing meshUpdate");
        mesh.update();
    }
}
#endif