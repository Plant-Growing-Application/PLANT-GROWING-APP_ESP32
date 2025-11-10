#pragma once

#include <WiFi.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <esp_wps.h>

class MyWiFi
{
public:
    MyWiFi(const char *prefsNamespace = "wifi"); // tek constructor

    // Başlat
    void begin(const char *ssid = nullptr, const char *password = nullptr);

    // Bağlan (timeout ms)
    bool connect(unsigned long timeoutMs);

    bool isConnected() const;
    bool isDHCP() const;
    String getLocalIPString() const;
    IPAddress getLocalIP() const;

    // Ağ konfigürasyonu
    void useDHCP();
    bool setStaticIP(const char *localIP, const char *gateway, const char *subnet, const char *dns);
    bool saveSettings();
    void setSSID(const char *ssid);
    void setPassword(const char *pass);
    const char *getSSID();
    const char *getPassword();
    void clearSettings();
    bool applyConfig();

    // WPS / event handler
    void attachWpsHandler(); // WiFi.onEvent(...) bağlar — setup içinde çağır
    void startWPS();         // WPS PBC başlatır
    bool isWpsActive() const { return _wpsActive; }

private:
    // event callback (member function)
    void onWiFiEvent(WiFiEvent_t event);

    // helper
    bool parseIP(const char *ipStr, IPAddress &out);

    // prefs
    const char *_prefsNs;
    Preferences _prefs;

    // credentials
    String _ssidStr;
    String _passwordStr;
    const char *_ssid;
    const char *_password;

    // network
    bool _useDHCP;
    IPAddress _localIP, _gateway, _subnet, _dns;

    // static WPS state + config
    static bool _wpsActive;
    static esp_wps_config_t _wps_config;
};
