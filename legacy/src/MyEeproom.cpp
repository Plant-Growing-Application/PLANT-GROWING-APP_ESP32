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
void MyEEPROM::ResetEeprom()
{
    StoredData sd;
    // EEPROM başlat
    EEPROM.begin(sizeof(StoredData));
    // String alanları temizle
    memset(sd.setting.SSID, 0, sizeof(sd.setting.SSID));
    memset(sd.setting.Password, 0, sizeof(sd.setting.Password));
    memset(sd.setting.IP, 0, sizeof(sd.setting.IP));
    memset(sd.setting.MAC, 0, sizeof(sd.setting.MAC));
    // Bool değerleri default
    sd.setting.IsServerMode = true;
    sd.setting.IsWpsActive = false;
    // Magic numarası
    sd.magic = MAGIC;

    // EEPROM'a yaz ve commit et
    EEPROM.put(_address, sd);
    EEPROM.commit();

    Serial.println("✅ EEPROM resetlendi");
}


// Global nesne
MyEEPROM MyEeprom;
