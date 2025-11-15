// ------------------- MyEEPROM.h -------------------
#ifndef MYEEPROM_H
#define MYEEPROM_H

#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h> // IPAddress için gerekli

typedef struct
{
    char SSID[32];
    char Password[32];
    char IP[16];
    char MAC[16];
    bool IsServerMode;
    bool IsBluetoothActive;
    bool IsWpsActive;
} Settings;

struct StoredData {
    uint32_t magic;
    Settings setting;
};

class MyEEPROM
{
public:
    Settings Setting;

    void Begin();  // EEPROM başlatma
    void SaveSettings(const Settings &Setting);
    bool GetSettings(Settings &outSetting);
    void ResetEeprom();
    void SaveIP(IPAddress ip, int ipOffset); // parametre ipOffset ile
    IPAddress GetIP(int ipOffset);

    int _address = 0;

private:
    static const uint32_t MAGIC = 0x5A5A5A5A;
};

extern MyEEPROM MyEeprom;

#endif /* MYEEPROM_H */
