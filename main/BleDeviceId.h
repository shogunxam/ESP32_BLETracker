#ifndef __BleDeviceId_H__
#define __BleDeviceId_H__
#include <Arduino.h>
#include <cstring>

class BleDeviceId
{
public:
    constexpr static int RAW_ID_SIZE = 16;
    constexpr static int MAC_ID_SIZE = 6;
    constexpr static int UUID_STRING_SIZE = 37; // 36 characters for UUID + null terminator

    BleDeviceId()
    {
        memset(m_raw, 0, RAW_ID_SIZE);
        m_isiBeacon = false;
    }

    BleDeviceId(const char* id)
    {
        fromString(id);
    }

    BleDeviceId(const uint8_t *genericId, bool isiBeacon)
    {
        if (isiBeacon)
        {
            buildFromiBeacon(genericId);
        }
        else
        {
            buildFromMAC(genericId);
        }
    }

    void buildFromMAC(const uint8_t mac[MAC_ID_SIZE])
    {
        memset(m_raw, 0, RAW_ID_SIZE);
        memcpy(m_raw, mac, MAC_ID_SIZE);
        m_isiBeacon = false;
    }

    void buildFromiBeacon(const uint8_t uuid[16])
    {
        memcpy(m_raw, uuid, RAW_ID_SIZE);
        m_isiBeacon = true;
    }

    bool isIBeacon() const
    {
        return m_isiBeacon;
    }

    const uint8_t *getRawID() const
    {
        return m_raw;
    }

    void toString(char output[UUID_STRING_SIZE]) const;

    String toString() const;

    void fromString(const char* input);

    // Operatore di confronto richiesto da std::map per l'ordinamento delle chiavi
    bool operator<(const BleDeviceId &other) const
    {
        if (m_isiBeacon != other.m_isiBeacon)
        {
            return m_isiBeacon < other.m_isiBeacon; // Ordina prima per tipo
        }
        return memcmp(m_raw, other.m_raw, RAW_ID_SIZE) < 0; // Poi esegue il confronto binario ultra-veloce
    }

    // Operatore di uguaglianza per semplificare i confronti nel codice
    bool operator==(const BleDeviceId &other) const
    {
        return (m_isiBeacon == other.m_isiBeacon) && (memcmp(m_raw, other.m_raw, RAW_ID_SIZE) == 0);
    }

    bool operator!=(const BleDeviceId &other) const
    {
        return !(*this == other);
    }

    // Operatore di uguaglianza per semplificare i confronti nel codice
    BleDeviceId & operator=(const BleDeviceId &other)
    {
        if (this != &other)
        {
            m_isiBeacon = other.m_isiBeacon;
            memcpy(m_raw, other.m_raw, RAW_ID_SIZE);
        }
        return *this;
    }

private:
    uint8_t m_raw[16]; // 6 byte per MAC (con padding di zeri) o 16 byte per UUID iBeacon
    bool m_isiBeacon;  // Distingue il tipo di identificativo
};

#endif /* __BleDeviceId_H__ */