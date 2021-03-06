#include "utility.h"
//Convert mac adddress from ABCEF1234567 to AB:CE:F1:23:45:67 or ab:ce:f1:23:45:67
void CanonicalAddress(const char address[ADDRESS_STRING_SIZE], char out[ADDRESS_STRING_SIZE + 5] , bool upperCase)
{
  char *outWlkr = out;
  for (int i = 0; i < ADDRESS_STRING_SIZE; i++)
  {
    *outWlkr = upperCase ? toupper(address[i]) : tolower(address[i]);
    outWlkr++;
    if ((i & 1) == 1 && i != (ADDRESS_STRING_SIZE - 2))
    {
      *outWlkr = ':';
      outWlkr++;
    }
  }
}
//Convert mac adddress to ABCEF1234567
void NormalizeAddress(const uint8_t address[6], char out[ADDRESS_STRING_SIZE])
{
  snprintf(out, ADDRESS_STRING_SIZE,"%02X%02X%02X%02X%02X%02X", address[0], address[1], address[2], address[3], address[4], address[5]);
}

//Convert mac adddress from AB:CE:F1:23:45:67 or ab:ce:f1:23:45:67 to ABCEF1234567
void NormalizeAddress(const char* in, char out[ADDRESS_STRING_SIZE])
{
    char *outWlkr = out;
    for (int i = 0; in[i]!='\0'; i++)
    {
      const char c = in[i];
      if (c == ':')
        continue;
      *outWlkr = toupper(c);
      outWlkr++;
    }
    out[ADDRESS_STRING_SIZE - 1] = '\0';
}

//Convert mac adddress from AB:CE:F1:23:45:67 or ab:ce:f1:23:45:67 to ABCEF1234567
void NormalizeAddress(const std::string &in, char out[ADDRESS_STRING_SIZE])
{
  NormalizeAddress(in.c_str(),out);
}