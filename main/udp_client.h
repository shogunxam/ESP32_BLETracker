#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H
#include "main.h"
#if USE_UDP
namespace UDPClient
{
    void initializeUDP();
    void publishBLEState(const BLETrackedDevice &device);
    void publishSySInfo();
}
#endif // USE_UDP
#endif // UDP_CLIENT_H