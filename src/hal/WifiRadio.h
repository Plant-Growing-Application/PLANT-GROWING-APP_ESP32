#pragma once

// Wi-Fi radyo sürücüsü ve olay köprüsü — TASK-034
//
// ── RADYOYA TEK KAPI (ARCHITECTURE P2) ──────────────────────────────────────
// `WiFi.*` ve `esp_wifi_*` çağrıları YALNIZCA `WifiRadio.cpp` içinde bulunur
// ve yalnızca `net` task'ından çalışır. Röle için uygulanan tek kapı kuralının
// ağ karşılığıdır ve aynı şekilde kod taramasıyla denetlenir.
//
// Tarama (TASK-039) ve SoftAP (TASK-038) da radyoya bu kapıdan erişir —
// istisna yoktur.
//
// ── OLAY KÖPRÜSÜ ────────────────────────────────────────────────────────────
// Wi-Fi event handler'ı **sistem task bağlamında** çalışır. Eski sistemde bu
// handler'ın içinde EEPROM yazma yapılıyordu: yavaş bir flash işlemi, kritik
// bir bağlamda. Burada handler yalnızca kuyruğa koyar ve hemen döner.
//
// Handler içinde YASAK: log, flash, bloklama, StateStore erişimi.
//
// ── D6: SÜRÜCÜDE İŞ KURALI YOK ──────────────────────────────────────────────
// Kopma nedeninin **sınıflandırılması** bir politika kararıdır ve burada
// DEĞİL, `services/network/NetworkEvents.h` içindedir. Bu sürücü ham
// `wifi_err_reason_t` değerini olduğu gibi taşır.

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/Time.h"

namespace hal {
namespace wifi {

/// Olay kuyruğu kapasitesi. `net` task'ı 100 ms'de bir tüketir; 8 slot tek
/// döngüde gelebilecek olay sayısının çok üstündedir.
constexpr uint8_t EVENT_QUEUE_LEN = 8;

/// Radyo modu.
///
/// `OFF` bilinçli olarak `DISABLED` değildir: `esp32-hal-gpio.h:58` içinde
/// `#define DISABLED 0x00` vardır ve `enum class` kapsamı preprocessor'a
/// karşı koruma SAĞLAMAZ (ISSUE-009).
enum class RadioMode : uint8_t
{
    OFF    = 0,
    STA    = 1,
    AP     = 2,
    AP_STA = 3,
};

/// Sürücünün ürettiği ham olay türleri — ESP-IDF olaylarının birebir karşılığı.
enum class WifiEvent : uint8_t
{
    NONE           = 0,
    STA_STARTED    = 1,
    STA_CONNECTED  = 2,  ///< AP'ye bağlanıldı — IP HENÜZ YOK
    STA_GOT_IP     = 3,  ///< IP alındı — bağlantı GERÇEKTEN kullanılabilir
    STA_LOST_IP    = 4,
    STA_DISCONNECT = 5,
    AP_STARTED     = 6,
    AP_STOPPED     = 7,
    AP_CLIENT_JOIN = 8,
    AP_CLIENT_LEFT = 9,
    SCAN_DONE      = 10,
};

/// Bir olay kaydı. POD, 12 bayt.
struct WifiEventRecord
{
    core::Millis at;
    uint16_t     reasonRaw;  ///< ham `wifi_err_reason_t`; sınıflandırma L2'de
    WifiEvent    event;
    uint8_t      detail;     ///< istemci sayısı / bulunan ağ sayısı
};

/// Radyoyu başlatır ve olay handler'ını kaydeder. Radyo `OFF` durumda kalır.
core::ErrCode begin();

/// Mod geçişi. **Atomik**: yarım kalmış geçiş radyoyu tanımsız bırakır.
core::ErrCode setMode(RadioMode mode);

RadioMode mode();

/// STA bağlantısını başlatır — **BLOKLAMAZ**. Sonuç olay olarak gelir.
///
/// Eski sistemde `connect(5000)` çağrısı task'ı 5 saniye blokluyordu ve bu,
/// watchdog beslemesinden SONRA yapıldığı için watchdog tarafından da
/// görülmüyordu.
core::ErrCode staConnect(const char* ssid, const char* password);

/// STA bağlantısını keser. Credential değişiminde temiz geçiş için gerekli.
core::ErrCode staDisconnect();

/// Statik IP uygular. **Bağlantı başlatılmadan ÖNCE** çağrılmalıdır;
/// sonradan uygulanması etkisiz kalır.
core::ErrCode staStaticIp(uint32_t ip, uint32_t gateway, uint32_t subnet, uint32_t dns);

/// DHCP'ye döner.
core::ErrCode staUseDhcp();

/// SoftAP'yi açar.
core::ErrCode apStart(const char* ssid, const char* password, uint32_t ip, uint32_t subnet);

core::ErrCode apStop();

/// Asenkron tarama başlatır. Sonuç `SCAN_DONE` olayıyla bildirilir.
core::ErrCode scanStart(bool showHidden);

/// Tarama sonucundan bir kaydı okur. `scanRelease()` çağrılana kadar geçerli.
bool scanResult(uint8_t index, char* ssidOut, size_t ssidLen, int8_t& rssi, uint8_t& channel,
                uint8_t& encType);

/// Tarama sonuç belleğini serbest bırakır.
void scanRelease();

// --- Durum sorguları --------------------------------------------------------

bool     staConnected();
uint32_t localIp();
uint32_t gatewayIp();
uint32_t subnetMask();
uint32_t dnsIp();
int8_t   rssi();
uint8_t  apClientCount();
void     macAddress(uint8_t out[6]);

/// Çip kimliğinden türetilen cihaza özgü son 3 MAC baytı — AP SSID/şifre
/// üretiminde kullanılır (TASK-038).
uint32_t deviceId();

// --- Olay kuyruğu -----------------------------------------------------------

/// Kuyruktan bir olay alır. `net` task'ından çağrılır.
bool popEvent(WifiEventRecord& out);

/// Kuyruk taşması nedeniyle kaybedilen olay sayısı.
///
/// Kritik olay (disconnect) kaybı FSM'i tutarsız bırakır; sayaç sıfırdan
/// büyükse bu bir teşhis bulgusudur — sessiz kayıp yoktur.
uint32_t droppedEvents();

} // namespace wifi
} // namespace hal
