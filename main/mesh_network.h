#ifndef _MESH_NETWORK_H_
#define _MESH_NETWORK_H_
#include <stdint.h>
namespace meshNetwork
{
    void setup();
    void publish();
    void publishNodeId();
    uint32_t getNodeId();
    bool hasIP();
    void loop();
    void meshUpdate();
}
#endif /*_MESH_NETWORK_H_*/