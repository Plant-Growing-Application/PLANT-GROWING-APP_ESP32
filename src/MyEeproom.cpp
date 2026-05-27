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
    // EEPROM başlat
    EEPROM.begin(sizeof(MyEeprom.Setting));
    // String alanları temizle
    memset(MyEeprom.Setting.SSID, 0, sizeof(MyEeprom.Setting.SSID));
    memset(MyEeprom.Setting.Password, 0, sizeof(MyEeprom.Setting.Password));
    memset(MyEeprom.Setting.IP, 0, sizeof(MyEeprom.Setting.IP));
    memset(MyEeprom.Setting.MAC, 0, sizeof(MyEeprom.Setting.MAC));
    // Bool değerleri default
    MyEeprom.Setting.IsServerMode = true;
    MyEeprom.Setting.IsWpsActive = false;

    // EEPROM'a yaz ve commit et
    EEPROM.put(0, MyEeprom.Setting);
    EEPROM.commit();

    Serial.println("✅ EEPROM resetlendi");
}


// Global nesne
MyEEPROM MyEeprom;
