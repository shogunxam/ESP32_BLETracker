#include "BleDeviceId.h"

void BleDeviceId::toString(char output[UUID_STRING_SIZE]) const
{
  static const char hexChars[] = "0123456789ABCDEF";
  if (m_isiBeacon) {
    // E2C56DB5-DFFB-48D2-B060-D0F5A71096E0
    int dst = 0;
    for (int i = 0; i < 16; i++) {
      if (i == 4 || i == 6 || i == 8 || i == 10) {
        output[dst++] = '-';
      }
      output[dst++] = hexChars[(m_raw[i] >> 4) & 0x0F];
      output[dst++] = hexChars[m_raw[i] & 0x0F];
    }
    output[dst] = '\0';
  } else {
    int dst = 0;
    for (int i = 0; i < 6; i++) {
      //if (i > 0) output[dst++] = ':';
      output[dst++] = hexChars[(m_raw[i] >> 4) & 0x0F];
      output[dst++] = hexChars[m_raw[i] & 0x0F];
    }
    output[dst] = '\0';
  }
}   

// Funzione helper inline per convertire un carattere esadecimale in valore intero (0-15)
// Gestisce sia maiuscole che minuscole in modo efficiente
static inline uint8_t hexCharToByte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0; // Fallback per caratteri non validi
}

// NOTA: Ho rimosso 'const' dalla firma del metodo, poiché modifica lo stato dell'oggetto (m_isiBeacon e m_raw)
void BleDeviceId::fromString(const char* input) 
{
    memset(m_raw, 0, sizeof(m_raw));
    size_t len = strlen(input);
    
    // 1. Determina il formato in base alla lunghezza (come richiesto)
    m_isiBeacon = (len == UUID_STRING_SIZE - 1) ? true : false;
    
    // 2. Quanti byte dobbiamo leggere? 6 per MAC (17 char), 16 per iBeacon/Raw (36 o 32 char)
    int target_bytes = (m_isiBeacon) ? RAW_ID_SIZE : MAC_ID_SIZE;
    int dst = 0;

    // 3. Unico ciclo per tutti i formati
    for (size_t i = 0; i < len && dst < target_bytes; i++) {
        char c = input[i];
        
        // Salta qualsiasi separatore (hypen o colon)
        if (c == '-' || c == ':') {
            continue;
        }
        
        // Assicurati che ci sia un secondo carattere per formare la coppia del byte
        if (i + 1 < len) {
            m_raw[dst++] = (hexCharToByte(c) << 4) | hexCharToByte(input[i+1]);
            i++; // Salta il secondo carattere della coppia (il for loop farà un altro i++)
        }
    }
}

String BleDeviceId::toString() const
    {
        char buffer[BleDeviceId::UUID_STRING_SIZE]; // 36 characters for UUID + null terminator
        toString(buffer);
        return String(buffer);
    }