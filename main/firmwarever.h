#ifndef __FIRMWAREVER_h__
#define __FIRMWAREVER_h__
#include <WString.h>
#define VERSION "3.9R"
namespace Firmware{
    
extern const char BuildTime[];
String FullVersion();
void writeVersion();
String readVersion();
}
#endif /*__FIRMWAREVER_h__*/
