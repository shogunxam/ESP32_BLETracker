#include <mutex>
char _printbuffer_[256];
std::mutex _printLock_;