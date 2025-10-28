#include "define.h"

MyWiFi::MyWiFi(const char* prefsNamespace)
    : _ssid(nullptr), _password(nullptr), _prefsNs(prefsNamespace),
      _useDHCP(true),
      _localIP(0,0,0,0), _gateway(0,0,0,0), _subnet(0,0,0,0), _dns(0,0,0,0)
{
}

void MyWiFi::begin(const char* ssid, const char* password)
{
    _ssid = ssid;
    _password = password;

    _prefs.begin(_prefsNs, false);

    if (_prefs.isKey("mode"))
    {
        int mode = _prefs.getInt("mode", 1);
        if (mode == 0)
        {
            _useDHCP = true;
        }
        else
        {
            _useDHCP = false;
            String lip = _prefs.getString("lip", "");
            String gw  = _prefs.getString("gw", "");
            String sn  = _prefs.getString("sn", "");
            String dn  = _prefs.getString("dn", "");
            parseIP(lip.c_str(), _localIP);
            parseIP(gw.c_str(), _gateway);
            parseIP(sn.c_str(), _subnet);
            if (dn.length()) parseIP(dn.c_str(), _dns);
        }
    }
}

void MyWiFi::useDHCP()
{
    _useDHCP = true;
}

bool MyWiFi::parseIP(const char* ipStr, IPAddress &out)
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

bool MyWiFi::setStaticIP(const char* localIP, const char* gateway, const char* subnet, const char* dns)
{
    IPAddress lip, gw, sn, dn;
    if (!parseIP(localIP, lip)) return false;
    if (!parseIP(gateway, gw)) return false;
    if (!parseIP(subnet, sn)) return false;
    if (dns && strlen(dns) > 0)
    {
        if (!parseIP(dns, dn)) return false;
    }
    else dn = IPAddress(8,8,8,8);

    _useDHCP = false;
    _localIP = lip;
    _gateway = gw;
    _subnet  = sn;
    _dns     = dn;
    return true;
}

void MyWiFi::saveSettings()
{
    _prefs.putInt("mode", _useDHCP ? 0 : 1);
    if (!_useDHCP)
    {
        _prefs.putString("lip", _localIP.toString());
        _prefs.putString("gw",  _gateway.toString());
        _prefs.putString("sn",  _subnet.toString());
        _prefs.putString("dn",  _dns.toString());
    }
    else
    {
        _prefs.clear();
    }
}

void MyWiFi::clearSettings()
{
    _prefs.clear();
    _useDHCP = true;
}

bool MyWiFi::applyConfig()
{
    if (_useDHCP)
        return true;
    return WiFi.config(_localIP, _gateway, _subnet, _dns);
}

bool MyWiFi::connect(unsigned long timeoutMs)
{
    if (!_ssid) return false;

    applyConfig();
    WiFi.begin(_ssid, _password);

    unsigned long start = millis();
    while (millis() - start < timeoutMs)
    {
        if (WiFi.status() == WL_CONNECTED)
            return true;
        delay(100);
    }
    return (WiFi.status() == WL_CONNECTED);
}

void MyWiFi::connectNoConfig(unsigned long timeoutMs)
{
    if (!_ssid) return;
    WiFi.begin(_ssid, _password);
    unsigned long start = millis();
    while (millis() - start < timeoutMs)
    {
        if (WiFi.status() == WL_CONNECTED)
            break;
        delay(100);
    }
}

bool MyWiFi::isDHCP() const
{
    return _useDHCP;
}

bool MyWiFi::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

String MyWiFi::getLocalIPString() const
{
    return WiFi.localIP().toString();
}

IPAddress MyWiFi::getLocalIP() const
{
    return WiFi.localIP();
}
