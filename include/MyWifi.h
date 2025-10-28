#ifndef MYWIFI_H
#define MYWIFI_H
#include "define.h"

class MyWiFi
{
public:
    MyWiFi(const char *prefsNamespace = "wifi");

    void begin(const char *ssid, const char *password);
    void useDHCP();
    bool setStaticIP(const char *localIP, const char *gateway, const char *subnet, const char *dns = nullptr);
    void saveSettings();
    void clearSettings();
    bool connect(unsigned long timeoutMs = 15000);
    void connectNoConfig(unsigned long timeoutMs = 15000);
    bool isDHCP() const;
    bool isConnected() const;
    String getLocalIPString() const;
    IPAddress getLocalIP() const;

private:
    const char *_ssid;
    const char *_password;
    Preferences _prefs;
    const char *_prefsNs;

    bool _useDHCP;
    IPAddress _localIP;
    IPAddress _gateway;
    IPAddress _subnet;
    IPAddress _dns;

    bool parseIP(const char *ipStr, IPAddress &out);
    bool applyConfig();
};

extern MyWiFi wifi;

#endif
