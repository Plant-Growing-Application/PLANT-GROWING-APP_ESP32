#pragma once

// Web sunucusu iskeleti — TASK-041
//
// ── ASYNCTCP BAĞLAM KURALLARI (§14.6) — PAZARLIKSIZ ─────────────────────────
//
//   | Kural                                   | Gerekçe                        |
//   |-----------------------------------------|--------------------------------|
//   | Callback'te dosya taraması/uzun döngü YOK| AsyncTCP task'ını bloklar,     |
//   |                                         | TÜM web donar                  |
//   | Callback yalnızca doğrular + kuyruğa koyar| Komut yürütme `app_core`'un    |
//   | Büyük JSON önceden boyutlandırılmış     | Heap parçalanmasını önler      |
//   | Yavaş istemci sistemi bloklamamalı      | Yazma kuyruğu doluysa düşürülür|
//
// Eski sistemde WebSocket handler'ı doğrudan `digitalWrite()` yapıyordu:
// AsyncTCP bağlamından, güvenlik kontrolü olmadan pompa sürülüyordu.
//
// ── SUNUCU AP VE STA FARK ETMEKSİZİN BAŞLAR ─────────────────────────────────
// Kurulum tam olarak AP modunda yapılır; sunucunun yalnızca STA bağlıyken
// başlaması, cihazın hiç yapılandırılamaması demektir.

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/Time.h"

class AsyncWebServer;

namespace interfaces {
namespace web {

constexpr uint16_t HTTP_PORT = 80;

/// Rotaları, WebSocket'i ve statik servisi KAYDEDER — **dinlemeye BAŞLAMAZ**.
///
/// Dosya sistemi mount edilmemiş olsa bile kurulur: statik istekler için
/// "dosya sistemi kullanılamıyor" diyen açık bir yanıt döner. Sessiz 404
/// kullanıcıyı yanlış yerde arattırır.
core::ErrCode begin();

/// Dinlemeye başlar. **TCP/IP yığını HAZIR olduktan sonra çağrılmalıdır.**
///
/// ── NEDEN AYRI ──────────────────────────────────────────────────────────
/// `AsyncServer::begin()` lwIP'in `tcpip_api_call()`'unu kullanır. Radyo
/// `WIFI_MODE_NULL` iken lwIP TCP/IP thread'i HİÇ BAŞLATILMAMIŞ olur ve
/// çağrı `assert failed: tcpip_api_call ... (Invalid mbox)` ile PANİK atar.
///
/// İlk sahada bu bir BOOT DÖNGÜSÜ olarak görüldü: `net` task'ı radyoyu bir
/// moda almadan (ilk `fsm::tick()` öncesi) sunucuyu başlatıyordu.
///
/// Çağıran, radyonun `OFF` dışında bir modda olduğunu doğrulamalıdır.
/// Yeniden çağrılması güvenlidir (etkisiz).
core::ErrCode start();

/// Sunucu dinliyor mu? Eski sistemde `begin()` dönüşü hiç kontrol edilmiyordu.
bool listening();

/// Rotaların bağlanabilmesi için sunucu nesnesi.
/// **Yalnızca kayıt sırasında**, `begin()` öncesinde kullanılmalıdır.
AsyncWebServer& server();

/// Periyodik bakım — `net` task'ından çağrılır.
///
/// Eski sistemde `cleanupClients()` `loop()` içindeydi; artık ağ task'ının
/// sorumluluğunda.
void tick(core::Millis now);

} // namespace web
} // namespace interfaces
