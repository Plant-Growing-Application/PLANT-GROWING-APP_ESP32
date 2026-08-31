#include "hal/WifiRadio.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <string.h>

namespace hal {
namespace wifi {
namespace {

using core::ErrCode;
using core::Millis;

// --- Olay kuyruğu -----------------------------------------------------------
//
// Sistem task'ı yazar, `net` task'ı okur. Tek üretici / tek tüketici halka
// tamponu; `volatile` indeksler ve 32-bit hizalı erişimle kilitsiz çalışır.
// Kritik bölüm YOK: event handler'ın kilit beklemesi tam olarak kaçındığımız
// şeydir.

WifiEventRecord   g_ring[EVENT_QUEUE_LEN];
volatile uint8_t  g_head    = 0;   ///< yazar
volatile uint8_t  g_tail    = 0;   ///< okuyucu
volatile uint32_t g_dropped = 0;

RadioMode g_mode  = RadioMode::OFF;
bool      g_ready = false;

inline uint8_t next(uint8_t i) { return static_cast<uint8_t>((i + 1u) % EVENT_QUEUE_LEN); }

/// Kuyruğa koyar. **Handler bağlamından çağrılır** — log yok, flash yok,
/// bloklama yok.
void push(WifiEvent ev, uint16_t reasonRaw, uint8_t detail)
{
    const uint8_t h = g_head;
    const uint8_t n = next(h);
    if (n == g_tail)
    {
        // Kuyruk dolu. Sessizce kaybetmiyoruz: sayaç yayınlanır ve
        // sıfırdan büyük olması bir teşhis bulgusudur.
        ++g_dropped;
        return;
    }

    g_ring[h].at        = Millis{millis()};
    g_ring[h].reasonRaw = reasonRaw;
    g_ring[h].event     = ev;
    g_ring[h].detail    = detail;
    g_head              = n;
}

/// Arduino Wi-Fi olay köprüsü. TEK İŞİ kuyruğa koymaktır.
void onWifiEvent(arduino_event_id_t id, arduino_event_info_t info)
{
    switch (id)
    {
        case ARDUINO_EVENT_WIFI_STA_START:
            push(WifiEvent::STA_STARTED, 0, 0);
            break;

        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            push(WifiEvent::STA_CONNECTED, 0, 0);
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            push(WifiEvent::STA_GOT_IP, 0, 0);
            break;

        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            push(WifiEvent::STA_LOST_IP, 0, 0);
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            // Ham neden kodu KORUNUR. Sınıflandırma L2'nin işidir (D6).
            push(WifiEvent::STA_DISCONNECT,
                 static_cast<uint16_t>(info.wifi_sta_disconnected.reason), 0);
            break;

        case ARDUINO_EVENT_WIFI_AP_START:
            push(WifiEvent::AP_STARTED, 0, 0);
            break;

        case ARDUINO_EVENT_WIFI_AP_STOP:
            push(WifiEvent::AP_STOPPED, 0, 0);
            break;

        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            push(WifiEvent::AP_CLIENT_JOIN, 0, 0);
            break;

        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            push(WifiEvent::AP_CLIENT_LEFT, 0, 0);
            break;

        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            push(WifiEvent::SCAN_DONE, 0,
                 static_cast<uint8_t>(info.wifi_scan_done.number));
            break;

        default:
            break;   // ilgilenmediğimiz olaylar sessizce yok sayılır
    }
}

/// `RadioMode` → Arduino `wifi_mode_t`.
wifi_mode_t toArduino(RadioMode m)
{
    return (m == RadioMode::STA)    ? WIFI_MODE_STA
         : (m == RadioMode::AP)     ? WIFI_MODE_AP
         : (m == RadioMode::AP_STA) ? WIFI_MODE_APSTA
                                    : WIFI_MODE_NULL;
}

IPAddress toIp(uint32_t raw) { return IPAddress(raw); }

} // namespace

core::ErrCode begin()
{
    if (g_ready) { return ErrCode::OK; }

    g_head    = 0;
    g_tail    = 0;
    g_dropped = 0;

    WiFi.onEvent(onWifiEvent);

    // Otomatik yeniden bağlanmayı KAPAT: yeniden deneme politikası
    // TASK-037'nindir. Sürücünün kendi başına denemesi, backoff ve
    // kimlik-hatası sınırlaması ile çakışırdı (D6).
    WiFi.setAutoReconnect(false);
    WiFi.persistent(false);   // credential'ı NVS'e yazan sürücü davranışı kapalı
                              // — sırlar `SecretStore`'un işidir (TASK-013)

    if (!WiFi.mode(WIFI_MODE_NULL)) { return ErrCode::NET_DISCONNECTED; }
    g_mode = RadioMode::OFF;

    g_ready = true;
    return ErrCode::OK;
}

core::ErrCode setMode(RadioMode m)
{
    if (m == g_mode) { return ErrCode::OK; }

    // Atomik geçiş: `WiFi.mode()` tek çağrıda hedef moda geçer. Ara moddan
    // geçmek (örn. AP_STA → OFF → STA) radyoyu tanımsız bırakır ve bağlı
    // istemcileri gereksiz düşürür.
    if (!WiFi.mode(toArduino(m))) { return ErrCode::NET_DISCONNECTED; }

    g_mode = m;

    // Modem sleep KAPALI: açıkken ilk paket 100+ ms gecikir ve web arayüzü
    // yavaş hissettirir. Yapılandırılabilir yapılmadı — hiçbir yerden
    // değiştirilmeyen ölü bir config alanı olurdu (P7).
    if (m != RadioMode::OFF) { WiFi.setSleep(false); }

    return ErrCode::OK;
}

RadioMode mode() { return g_mode; }

core::ErrCode staConnect(const char* ssid, const char* password)
{
    if (ssid == nullptr || ssid[0] == '\0') { return ErrCode::NET_NO_CREDENTIALS; }

    // BLOKLAMAZ: `WiFi.begin()` isteği kuyruğa koyar, sonuç olay olarak gelir.
    // Eski sistemdeki `connect(5000)` beklemesi bilinçli olarak yoktur.
    const wl_status_t st = WiFi.begin(ssid, (password != nullptr && password[0] != '\0')
                                                ? password
                                                : nullptr);
    return (st == WL_CONNECT_FAILED) ? ErrCode::NET_DISCONNECTED : ErrCode::OK;
}

core::ErrCode staDisconnect()
{
    // `false` = radyoyu kapatma, yalnızca bağlantıyı kes.
    return WiFi.disconnect(false, false) ? ErrCode::OK : ErrCode::NET_DISCONNECTED;
}

core::ErrCode staStaticIp(uint32_t ip, uint32_t gateway, uint32_t subnet, uint32_t dns)
{
    return WiFi.config(toIp(ip), toIp(gateway), toIp(subnet), toIp(dns))
               ? ErrCode::OK
               : ErrCode::NET_IP_CONFIG_INVALID;
}

core::ErrCode staUseDhcp()
{
    // Tüm alanlar 0.0.0.0 → DHCP'ye dön.
    return WiFi.config(IPAddress(0u), IPAddress(0u), IPAddress(0u), IPAddress(0u))
               ? ErrCode::OK
               : ErrCode::NET_IP_CONFIG_INVALID;
}

core::ErrCode apStart(const char* ssid, const char* password, uint32_t ip, uint32_t subnet)
{
    if (ssid == nullptr || ssid[0] == '\0') { return ErrCode::NET_IP_CONFIG_INVALID; }

    if (!WiFi.softAPConfig(toIp(ip), toIp(ip), toIp(subnet)))
    {
        return ErrCode::NET_IP_CONFIG_INVALID;
    }
    return WiFi.softAP(ssid, password) ? ErrCode::OK : ErrCode::NET_DISCONNECTED;
}

core::ErrCode apStop()
{
    return WiFi.softAPdisconnect(false) ? ErrCode::OK : ErrCode::NET_DISCONNECTED;
}

core::ErrCode scanStart(bool showHidden)
{
    // async = true → bloklamaz; sonuç `SCAN_DONE` olayıyla gelir.
    const int16_t r = WiFi.scanNetworks(true, showHidden);
    return (r == WIFI_SCAN_FAILED) ? ErrCode::NET_SCAN_FAILED : ErrCode::OK;
}

bool scanResult(uint8_t index, char* ssidOut, size_t ssidLen, int8_t& rssiOut,
                uint8_t& channel, uint8_t& encType)
{
    const int16_t n = WiFi.scanComplete();
    if (n <= 0 || index >= static_cast<uint8_t>(n)) { return false; }

    const String s = WiFi.SSID(index);
    if (ssidOut != nullptr && ssidLen > 0)
    {
        strncpy(ssidOut, s.c_str(), ssidLen - 1);
        ssidOut[ssidLen - 1] = '\0';
    }
    rssiOut = static_cast<int8_t>(WiFi.RSSI(index));
    channel = static_cast<uint8_t>(WiFi.channel(index));
    encType = static_cast<uint8_t>(WiFi.encryptionType(index));
    return true;
}

void scanRelease() { WiFi.scanDelete(); }

bool     staConnected() { return WiFi.status() == WL_CONNECTED; }
uint32_t localIp()      { return static_cast<uint32_t>(WiFi.localIP()); }
uint32_t gatewayIp()    { return static_cast<uint32_t>(WiFi.gatewayIP()); }
uint32_t subnetMask()   { return static_cast<uint32_t>(WiFi.subnetMask()); }
uint32_t dnsIp()        { return static_cast<uint32_t>(WiFi.dnsIP()); }
int8_t   rssi()         { return staConnected() ? static_cast<int8_t>(WiFi.RSSI()) : 0; }
uint8_t  apClientCount(){ return static_cast<uint8_t>(WiFi.softAPgetStationNum()); }

void macAddress(uint8_t out[6])
{
    if (out != nullptr) { WiFi.macAddress(out); }
}

uint32_t deviceId()
{
    uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
    WiFi.macAddress(mac);
    return (static_cast<uint32_t>(mac[3]) << 16) | (static_cast<uint32_t>(mac[4]) << 8) |
           static_cast<uint32_t>(mac[5]);
}

bool popEvent(WifiEventRecord& out)
{
    const uint8_t t = g_tail;
    if (t == g_head) { return false; }

    out    = g_ring[t];
    g_tail = next(t);
    return true;
}

uint32_t droppedEvents() { return g_dropped; }

} // namespace wifi
} // namespace hal
