#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

class MyWiFi
{
public:
    MyWiFi(const char *prefsNamespace);

    void begin(const char *ssid, const char *password);
    bool connect(unsigned long timeoutMs = 5000);
    void useDHCP();
    bool setStaticIP(const char *localIP, const char *gateway, const char *subnet, const char *dns = nullptr);
    void saveSettings();
    void clearSettings();

    bool isDHCP() const;
    bool isConnected() const;

    String getLocalIPString() const;
    IPAddress getLocalIP() const;
    const char *_ssid;
    const char *_password;
    const char *_prefsNs;

private:
    bool _useDHCP;
    IPAddress _localIP, _gateway, _subnet, _dns;

    Preferences _prefs;

    bool parseIP(const char *ipStr, IPAddress &out);
    bool applyConfig();
};
