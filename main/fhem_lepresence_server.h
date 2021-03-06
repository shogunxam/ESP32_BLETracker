#ifndef __FHEM_LEPRESENCE_SERVER_H__
#define __FHEM_LEPRESENCE_SERVER_H__
namespace FHEMLePresenceServer
{
    void initializeServer();
    void startAsyncServer();
    //Call in main loop if you don't use the async server
    void loop();
};
#endif /*__FHEM_LEPRESENCE_SERVER_H__*/