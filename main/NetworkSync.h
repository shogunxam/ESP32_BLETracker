#ifndef _NETWORKSYNC_H_
#define _NETWORKSYNC_H_
#include "myMutex.h"
namespace NetworkSync{
extern MyMutex networkLock;
}
#define NetworkAcquire() CRITICALSECTION_START(NetworkSync::networkLock) 
#define NetworkRelease() CRITICALSECTION_END 
#endif /*_NETWORKSYNC_H_*/