#ifndef __UTILITY_H__
#define __UTILITY_H__
#include "main.h"
#include <BLEAddress.h>
//Convert mac adddress from ABCEF1234567 to AB:CE:F1:23:45:67 or ab:ce:f1:23:45:67
void CanonicalAddress(const char address[ADDRESS_STRING_SIZE], char out[ADDRESS_STRING_SIZE + 5] , bool upperCase = true);
void NormalizeAddress(const std::string &in, char out[ADDRESS_STRING_SIZE]);
void NormalizeAddress(const char* in, char out[ADDRESS_STRING_SIZE]);
void NormalizeAddress(const uint8_t address[6], char out[ADDRESS_STRING_SIZE]);
#endif /*__UTILITY_H__*/