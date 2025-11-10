#include "MyWiFi.h"
#include <string.h> // memset

// statikler tanımla
bool MyWiFi::_wpsActive = false;
esp_wps_config_t MyWiFi::_wps_config;

// Constructor
MyWiFi::MyWiFi(const char *prefsNamespace)
    : _prefsNs(prefsNamespace),
      _ssid(nullptr), _password(nullptr),
      _useDHCP(true),
      _localIP(0, 0, 0, 0), _gateway(0, 0, 0, 0), _subnet(0, 0, 0, 0),
      _dns(8, 8, 8, 8)
{
    _ssidStr = "";
    _passwordStr = "";

    // güvenli WPS config init
    memset(&_wps_config, 0, sizeof(_wps_config));
    _wps_config.wps_type = WPS_TYPE_PBC;
}

// Başlat
void MyWiFi::begin(const char *ssid, const char *password)
{
    if (ssid && strlen(ssid) > 0)
    {
        _ssidStr = String(ssid);
        _ssid = _ssidStr.c_str();
    }
    if (password && strlen(password) > 0)
    {
        _passwordStr = String(password);
        _password = _passwordStr.c_str();
    }

    _prefs.begin(_prefsNs, false);

    if (_prefs.isKey("mode"))
    {
        int mode = _prefs.getInt("mode", 1);
        _useDHCP = (mode == 0);

        if (!_useDHCP)
        {
            String lip = _prefs.getString("lip", "");
            String gw = _prefs.getString("gw", "");
            String sn = _prefs.getString("sn", "");
            String dn = _prefs.getString("dn", "");

            if (lip.length())
                parseIP(lip.c_str(), _localIP);
            if (gw.length())
                parseIP(gw.c_str(), _gateway);
            if (sn.length())
                parseIP(sn.c_str(), _subnet);
            if (dn.length())
                parseIP(dn.c_str(), _dns);
        }
    }

    _prefs.end();
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

// Setter / Getter
void MyWiFi::setSSID(const char *ssid) { _ssidStr = String(ssid); _ssid = _ssidStr.c_str(); }
void MyWiFi::setPassword(const char *pass) { _passwordStr = String(pass); _password = _passwordStr.c_str(); }
const char *MyWiFi::getSSID() { return _ssid; }
const char *MyWiFi::getPassword() { return _password; }

// Kaydet / temizle
bool MyWiFi::saveSettings()
{
    _prefs.begin(_prefsNs, false);
    _prefs.putString("ssid", _ssidStr);
    _prefs.putString("pass", _passwordStr);
    _prefs.end();
    Serial.println("✅ WiFi bilgileri kaydedildi!");
    return true;
}

void MyWiFi::clearSettings()
{
    _prefs.begin(_prefsNs, false);
    _prefs.clear();
    _prefs.end();
    _useDHCP = true;
    _ssidStr = "";
    _passwordStr = "";
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
    if (_ssidStr.length() == 0)
    {
        _prefs.begin(_prefsNs, true);
        String ssid = _prefs.getString("ssid", "");
        String pass = _prefs.getString("pass", "");
        _prefs.end();

        if (ssid.length())
        {
            _ssidStr = ssid;
            _passwordStr = pass;
            _ssid = _ssidStr.c_str();
            _password = _passwordStr.c_str();
        }
    }

    if (WiFi.status() == WL_CONNECTED)
        return true;
    if (_ssidStr.length() == 0)
        return false;

    if (!_useDHCP)
        applyConfig();
    else
        WiFi.mode(WIFI_STA);

    WiFi.begin(_ssidStr.c_str(), _passwordStr.c_str());

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
    WiFi.onEvent([this](arduino_event_id_t event, arduino_event_info_t info) { this->onWiFiEvent(event); });
}

void MyWiFi::startWPS()
{
    _wpsActive = true;
    Serial.println("WPS Başlatılıyor...");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    delay(200);
    esp_wifi_wps_disable();

    esp_err_t r = esp_wifi_wps_enable(&_wps_config);
    if (r != ESP_OK)
    {
        Serial.printf("esp_wifi_wps_enable HATA: %d\n", r);
        return;
    }

    r = esp_wifi_wps_start(0);
    Serial.printf("esp_wifi_wps_start: %d\n", r);
}

void MyWiFi::onWiFiEvent(WiFiEvent_t event)
{
    switch (event)
    {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.print("IP: "); Serial.println(WiFi.localIP());
        _wpsActive = false;
        break;

    case ARDUINO_EVENT_WPS_ER_SUCCESS:
    {
        _wpsActive = false;
        Serial.println("WPS BAŞARILI - credential alınıyor...");
        wifi_config_t cfg;
        if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK)
        {
            String ssd = String((char *)cfg.sta.ssid);
            String pss = String((char *)cfg.sta.password);

            Preferences p;
            p.begin(_prefsNs, false);
            p.putString("ssid", ssd);
            p.putString("pass", pss);
            p.end();
        }
        else Serial.println("esp_wifi_get_config hata");

        esp_wifi_wps_disable();
        WiFi.begin();
        break;
    }

    case ARDUINO_EVENT_WPS_ER_FAILED:
    case ARDUINO_EVENT_WPS_ER_TIMEOUT:
        Serial.println("WPS BAŞARISIZ veya TIMEOUT");
        _wpsActive = false;
        esp_wifi_wps_disable();
        break;

    default: break;
    }
}
