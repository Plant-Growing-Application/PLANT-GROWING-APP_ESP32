#pragma once

#include <WiFi.h>
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
    bool applyConfig();

    // WPS / event handler
    void onWiFiEvent(WiFiEvent_t event);
    void StartWps(); // WPS PBC başlatır
    void ConnectFromWeb();
    void ConnectFromWPS(); // WPS buton kontrolü
    bool isWpsActive() const { return _wpsActive; }
    bool wifiShouldReconnect = false;

private:
    unsigned long backPressTime = 0;
    bool wpsRunning = false;

    // helper
    bool parseIP(const char *ipStr, IPAddress &out);

    // network
    bool _useDHCP;
    IPAddress _localIP, _gateway, _subnet, _dns;

    // static WPS state
    static bool _wpsActive;
};
extern MyWiFi MywiFi;
