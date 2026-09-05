#pragma once

// STA bağlantı yöneticisi — TASK-036
//
// ── BLOKLAMA YASAĞI ─────────────────────────────────────────────────────────
// `beginConnect()` isteği başlatır ve HEMEN döner. Sonuç olay olarak gelir.
// Eski sistemdeki `connect(5000)` çağrısı task'ı 5 saniye blokluyordu ve bu,
// watchdog beslemesinden SONRA yapıldığı için watchdog tarafından da
// görülmüyordu.
//
// ── ZAMAN AŞIMI BİR EMNİYET VALFİDİR ────────────────────────────────────────
// Normal yolda sonuç event olarak gelir. Zaman aşımı YALNIZCA event hiç
// gelmezse devreye girer. Böyle bir durumda FSM sonsuza kadar `CONNECTING`'de
// kalır, AP fallback hiç açılmaz ve kullanıcı cihaza HİÇ erişemez.
//
// ── ŞİFRENİN YOLCULUĞU ──────────────────────────────────────────────────────
// `SecretStore` → yığın tamponu → radyo. Şifre `SystemState`'e, log'a veya
// API yanıtına GİRMEZ; kullanımdan sonra tampon sıfırlanır — yığında kalan
// bir kopya stack dump'ında görünebilir.

#include <stdint.h>

#include "core/Config.h"
#include "core/ErrorCodes.h"
#include "core/Time.h"

namespace services {
namespace net {
namespace conn {

/// Event hiç gelmezse devreye giren emniyet valfi.
///
/// Normal yolda sonuç 2-8 sn içinde event olarak gelir (auth + DHCP). 12 sn
/// bunun kat kat üstüdür ve valfin erken tetiklenmesi bir denemeyi boşa
/// harcamaz. Önceki 20 sn, olay hiç gelmediği durumda kullanıcıyı 8 saniye
/// fazladan bekletiyordu — üstelik hiçbir şeyin OLMADIĞI 8 saniye.
constexpr uint32_t CONNECT_TIMEOUT_MS = 12000u;

core::ErrCode begin(const core::Config& cfg);

/// Kayıtlı bir SSID var mı? Yoksa FSM `CONNECTING`'e HİÇ girmemelidir —
/// boş SSID ile bağlanma denemesi anlamsız bir başarısızlık döngüsüdür.
bool hasCredentials();

/// Bağlantıyı başlatır. **Bloklamaz.**
///
/// IP planı bağlantı BAŞLATILMADAN ÖNCE uygulanır; sonradan uygulanması
/// etkisiz kalır.
core::ErrCode beginConnect(core::Millis now);

/// Bağlantıyı keser. Credential değişiminde temiz geçiş için gerekli —
/// yarım kalmış geçiş radyoyu tanımsız bırakır.
core::ErrCode abort();

/// Emniyet valfi doldu mu? `CONNECTING` durumunda her döngüde sorulur.
bool timedOut(core::Millis now);

/// Bağlantının kurulması ne kadar sürdü (ms)? Teşhis için değerli.
uint32_t lastConnectMs();

/// Denemenin başladığı an.
core::Millis attemptStartedAt();

/// Bağlantı kurulduğunda çağrılır — süre ölçümünü kapatır.
void onConnected(core::Millis now);

/// Yeni credential kaydeder ve mevcut bağlantıyı temiz şekilde kapatır.
core::ErrCode setCredentials(const char* ssid, const char* password);

/// "Ağı unut" — credential'ı GERÇEKTEN siler.
core::ErrCode forget();

} // namespace conn
} // namespace net
} // namespace services
