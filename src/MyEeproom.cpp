// ------------------- MyEEPROM.cpp -------------------
#include "define.h"

// EEPROM başlat
void MyEEPROM::Begin()
{
    EEPROM.begin(sizeof(StoredData));
}

void MyEEPROM::SaveSettings(const Settings &newSetting)
{
    StoredData sd;
    sd.magic = MAGIC;
    sd.setting = newSetting;
    sd.setting.SSID[sizeof(sd.setting.SSID) - 1] = '\0';
    sd.setting.Password[sizeof(sd.setting.Password) - 1] = '\0';
    EEPROM.writeBytes(_address, (uint8_t *)&sd, sizeof(StoredData));
    EEPROM.commit();
    Setting = sd.setting; // dahili kopyayı güncelle
}
// Ayarları oku
bool MyEEPROM::GetSettings(Settings &outSetting)
{
    StoredData sd;
    EEPROM.readBytes(_address, (uint8_t *)&sd, sizeof(StoredData));
    if (sd.magic != MAGIC)
        return false;
    outSetting = sd.setting;
    Setting = sd.setting; // dahili kopyayı güncelle
    return true;
}

// Ayarları temizle
void MyEEPROM::ClearSettings()
{
    StoredData sd;
    memset(&sd, 0, sizeof(StoredData));
    EEPROM.writeBytes(_address, (uint8_t *)&sd, sizeof(StoredData));
    EEPROM.commit();
}

// Global nesne
MyEEPROM MyEeprom;
