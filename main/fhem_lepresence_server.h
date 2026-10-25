#ifndef __FHEM_LEPRESENCE_SERVER_H__
#define __FHEM_LEPRESENCE_SERVER_H__
#include "BleDeviceId.h"

namespace FHEMLePresenceServer
{
    constexpr size_t VIRTUAL_MAC_ADDRESS_SIZE = 18;
    bool GetVirtualAddress(const BleDeviceId &deviceId, char output[VIRTUAL_MAC_ADDRESS_SIZE]);
    void initializeServer();
    void startAsyncServer();
    //Call in main loop if you don't use the async server
    void loop();
};
#endif /*__FHEM_LEPRESENCE_SERVER_H__*/