#include "define.h"
#include <string.h> // memset
Settings Setting;
// statikler tanımla
bool MyWiFi::_wpsActive = false;
esp_wps_config_t MyWiFi::_wps_config;
String _ssidStr;
String _passwordStr;
const char *_ssid;
const char *_password;
MyWiFi MywiFi("wifi");

// Constructor
MyWiFi::MyWiFi(const char *prefsNamespace)
    : _useDHCP(true), _localIP(0, 0, 0, 0), _gateway(0, 0, 0, 0), _subnet(0, 0, 0, 0), _dns(8, 8, 8, 8)
{
    // opsiyonel: prefsNamespace saklanabilir
}
void MyWiFi::begin(const char *ssid, const char *password)
{
    // Wi-Fi SSID ve şifreyi parametrelerden al
    if (ssid && strlen(ssid) > 0)
    {
        _ssidStr = String(ssid);
        _ssid = _ssidStr.c_str();
        snprintf(MyEeprom.Setting.SSID, sizeof(MyEeprom.Setting.SSID), "%s", ssid);
    }
    if (password && strlen(password) > 0)
    {
        _passwordStr = String(password);
        _password = _passwordStr.c_str();
        snprintf(MyEeprom.Setting.Password, sizeof(MyEeprom.Setting.Password), "%s", password);
    }

    // EEPROM'dan ayarları oku
    MyEeprom.GetSettings(MyEeprom.Setting);

    // DHCP mi yoksa Statik IP mi?
    _useDHCP = (MyEeprom.Setting.IsServerMode == 0);

    if (!_useDHCP)
    {

        Serial.println("Static IP Settings loaded from EEPROM:");
    }
    else
    {
        Serial.println("DHCP mode enabled (from EEPROM)");
    }

    // Şimdi bağlantıyı başlat
    if (_useDHCP)
    {
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }
    else
    {
        WiFi.config(_localIP, _gateway, _subnet, _dns);
    }

    WiFi.begin(_ssid, _password);
}

// Helper: IP parse
bool MyWiFi::parseIP(const char *ipStr, IPAddress &out)
{
    if (!ipStr || strlen(ipStr) < 7)
        return false;
    int parts[4];
    if (sscanf(ipStr, "%d.%d.%d.%d", &parts[0], &parts[1], &parts[2], &parts[3]) != 4)
        return false;
    for (int i = 0; i < 4; ++i)
        if (parts[i] < 0 || parts[i] > 255)
            return false;
    out = IPAddress(parts[0], parts[1], parts[2], parts[3]);
    return true;
}

// DHCP / Static
void MyWiFi::useDHCP() { _useDHCP = true; }

bool MyWiFi::setStaticIP(const char *localIP, const char *gateway, const char *subnet, const char *dns)
{
    IPAddress lip, gw, sn, dn;
    if (!parseIP(localIP, lip))
        return false;
    if (!parseIP(gateway, gw))
        return false;
    if (!parseIP(subnet, sn))
        return false;
    if (dns && strlen(dns) > 0)
        parseIP(dns, dn);
    else
        dn = IPAddress(8, 8, 8, 8);

    _useDHCP = false;
    _localIP = lip;
    _gateway = gw;
    _subnet = sn;
    _dns = dn;
    return true;
}

// Apply config
bool MyWiFi::applyConfig()
{
    if (_useDHCP)
        return true;
    if (_dns == IPAddress(0, 0, 0, 0))
        _dns = IPAddress(8, 8, 8, 8);

    WiFi.mode(WIFI_STA);
    return WiFi.config(_localIP, _gateway, _subnet, _dns);
}

// Connect
bool MyWiFi::connect(unsigned long timeoutMs)
{
    if (sizeof(MyEeprom.Setting.SSID) == 0)
        MyEeprom.Setting.IsServerMode = true;

    if (WiFi.status() == WL_CONNECTED)
        return true;
    if (sizeof(MyEeprom.Setting.SSID) == 0)
        return false;

    if (!_useDHCP)
        applyConfig();
    else
        WiFi.mode(WIFI_STA);

    WiFi.begin(MyEeprom.Setting.SSID, MyEeprom.Setting.Password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs)
        delay(100);

    return WiFi.status() == WL_CONNECTED;
}

// Status / IP
bool MyWiFi::isDHCP() const { return _useDHCP; }
bool MyWiFi::isConnected() const { return WiFi.status() == WL_CONNECTED; }
String MyWiFi::getLocalIPString() const { return WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "0.0.0.0"; }
IPAddress MyWiFi::getLocalIP() const { return WiFi.localIP(); }

// WPS
void MyWiFi::attachWpsHandler()
{
    WiFi.onEvent([this](arduino_event_id_t event, arduino_event_info_t info)
                 { this->onWiFiEvent(event); });
}

void MyWiFi::onWiFiEvent(WiFiEvent_t event)
{
    switch (event)
    {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        _wpsActive = false;
        break;

    case ARDUINO_EVENT_WPS_ER_SUCCESS:
    {
        _wpsActive = false;
        Serial.println("WPS BAŞARILI - credential alınıyor...");

        wifi_config_t cfg;
        if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK)
        {
            // WPS ile alınan SSID ve Password
            String ssid = String((char *)cfg.sta.ssid);
            String pass = String((char *)cfg.sta.password);

            // EEPROM’a kaydet
            MyEeprom.Setting.IsWpsActive = true; // flag
            strncpy(MyEeprom.Setting.SSID, ssid.c_str(), sizeof(MyEeprom.Setting.SSID));
            strncpy(MyEeprom.Setting.Password, pass.c_str(), sizeof(MyEeprom.Setting.Password));

            MyEeprom.SaveSettings(MyEeprom.Setting);
            Serial.println("SSID ve şifre EEPROM'a kaydedildi ✅");
        }
        else
        {
            Serial.println("esp_wifi_get_config hata ❌");
        }

        esp_wifi_wps_disable();
        WiFi.begin(); // kaydedilmiş credential ile bağlan
        break;
    }

    case ARDUINO_EVENT_WPS_ER_FAILED:
    case ARDUINO_EVENT_WPS_ER_TIMEOUT:
        Serial.println("WPS BAŞARISIZ veya TIMEOUT ❌");
        _wpsActive = false;
        MyEeprom.Setting.IsWpsActive = false;    // flag
        MyEeprom.SaveSettings(MyEeprom.Setting); // EEPROM’a kaydet
        esp_wifi_wps_disable();
        break;

    default:
        break;
    }
}

void MyWiFi::ConnectFromWeb()
{
    if (wifiShouldReconnect)
    {
        wifiShouldReconnect = false;

        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.begin(MyEeprom.Setting.SSID, MyEeprom.Setting.Password);
        MyEeprom.Setting.IsServerMode = false;
        MyEeprom.SaveSettings(MyEeprom.Setting);
        GrowPlant.GoToPageIntro();
        Serial.println(MyEeprom.Setting.SSID);

        // Opsiyonel: WiFi bağlanana kadar bekle
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
        {
            delay(100); // küçük delay → WDT tetiklemez
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.print("Connected! IP: ");
            Serial.println(WiFi.localIP());
        }
        else
        {
            Serial.println("Failed to connect.");
        }
    }
}

void MyWiFi::ConnectFromWPS()
{
    if (GrowPlant.CurrentPage != PAGE_WPS)
        return; // Sadece WPS sayfasında çalışsın

    static unsigned long wpsStartTime = 0;
    static bool wpsStarted = false;

    bool backState = !digitalRead(PIN_BACK_BUTTON);

    if (backState && !MywiFi.isConnected()) // Butona basılı
    {
        if (!wpsStarted)
        {
            // WPS başlat
            wpsManager.StartWps();
            wpsStarted = true;
            wpsStartTime = millis(); // zamanlayıcı başlat

            // OLED mesajı
            oled.fillRect(0, 35, 128, 16, SSD1306_BLACK); // eski yazıyı sil
            oled.setCursor(25, 35);
            oled.setTextColor(SSD1306_WHITE);
            oled.print("WPS Baslatildi");
            oled.display();
        }
        else
        {
            // 2 saniye içinde bağlanmadıysa hata göster
            if (!MywiFi.isConnected() && millis() - wpsStartTime > 2000)
            {
                oled.fillRect(0, 35, 128, 16, SSD1306_BLACK);
                oled.setCursor(30, 35);
                oled.setTextColor(SSD1306_WHITE);
                oled.print("WPS Hata!");
                oled.display();
            }
            else if (MywiFi.isConnected())
            {
                oled.fillRect(0, 35, 128, 16, SSD1306_BLACK);
                oled.setCursor(30, 35);
                oled.setTextColor(SSD1306_WHITE);
                oled.print("WPS Basarili");
                oled.display();
            }
        }
    }
    else // Buton bırakıldı
    {
        wpsStarted = false;
    }
}
