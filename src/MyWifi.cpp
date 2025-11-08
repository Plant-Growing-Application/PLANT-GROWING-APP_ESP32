#include "define.h"
#include "MyWifi.h"

MyWiFi::MyWiFi(const char *prefsNamespace)
    : _ssid(nullptr), _password(nullptr), _prefsNs(prefsNamespace),
      _useDHCP(true),
      _localIP(0, 0, 0, 0), _gateway(0, 0, 0, 0), _subnet(0, 0, 0, 0), _dns(8, 8, 8, 8)
{
}

void MyWiFi::begin(const char *ssid, const char *password)
{
    _ssid = ssid;
    _password = password;

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

            parseIP(lip.c_str(), _localIP);
            parseIP(gw.c_str(), _gateway);
            parseIP(sn.c_str(), _subnet);
            if (dn.length())
                parseIP(dn.c_str(), _dns);
        }
    }

    _prefs.end();
}

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

void MyWiFi::useDHCP()
{
    _useDHCP = true;
}

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

void MyWiFi::saveSettings()
{
    _prefs.begin(_prefsNs, false);
    _prefs.putInt("mode", _useDHCP ? 0 : 1);

    if (!_useDHCP)
    {
        _prefs.putString("lip", _localIP.toString());
        _prefs.putString("gw", _gateway.toString());
        _prefs.putString("sn", _subnet.toString());
        _prefs.putString("dn", _dns.toString());
    }
    else
    {
        _prefs.remove("lip");
        _prefs.remove("gw");
        _prefs.remove("sn");
        _prefs.remove("dn");
    }

    _prefs.end();
}

void MyWiFi::clearSettings()
{
    _prefs.begin(_prefsNs, false);
    _prefs.clear();
    _prefs.end();
    _useDHCP = true;
}

bool MyWiFi::applyConfig()
{
    if (_useDHCP)
        return true;
    if (_dns == IPAddress(0, 0, 0, 0))
        _dns = IPAddress(8, 8, 8, 8);

    WiFi.mode(WIFI_STA);
    return WiFi.config(_localIP, _gateway, _subnet, _dns);
}

bool MyWiFi::connect(unsigned long timeoutMs)
{
    if (WiFi.status() == WL_CONNECTED)
        return true; // zaten bağlı
    if (!_ssid)
        return false;

    if (!_useDHCP)
        applyConfig();
    else
        WiFi.mode(WIFI_STA);

    WiFi.begin(_ssid, _password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs)
        delay(100);

    return WiFi.status() == WL_CONNECTED;
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
    if (WiFi.status() != WL_CONNECTED)
        return "0.0.0.0";
    return WiFi.localIP().toString();
}

IPAddress MyWiFi::getLocalIP() const
{
    return WiFi.localIP();
}
