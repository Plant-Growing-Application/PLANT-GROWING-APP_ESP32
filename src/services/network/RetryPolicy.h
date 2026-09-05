#pragma once

// Yeniden deneme ve backoff politikası — TASK-037
//
// Eski sistemin yaklaşımı (sabit 1 sn, sonsuz) **varsayılan çözüm olarak
// alınmadı**. Yanlış şifreyle sonsuz deneme radyo enerjisi ve CPU harcar ve
// hiçbir zaman başarılı olmaz.
//
//   Taban 1 sn, ×2, TAVAN 20 sn:  1 → 2 → 4 → 8 → 16 → 20 → 20 ...
//   Jitter ±%20 — tavanı AŞMAZ.
//
// ── TAVAN NEDEN 60 SANİYE DEĞİL ─────────────────────────────────────────────
// Tavan önce 60 sn'ydi ve sahada şu yaşandı: internet birkaç saniyeliğine
// gitti, geri geldi — ama cihaz 60 saniyelik beklemenin ortasındaydı ve
// dokunacak bir şey yokken bir dakika daha "bağlantı yok" gösterdi.
//
// Backoff'un amacı, ULAŞILAMAYAN bir ağı sonsuz denemekten kaçınmaktır;
// kullanıcıyı bekletmek değil. 20 sn bu amacı korur (dakikada 3 deneme,
// radyo ve CPU için ihmal edilebilir) ve ağ döndüğünde en kötü ihtimalle
// 20 saniyede geri bağlanılır.
//
// **Jitter neden var:** aynı ortamdaki birden fazla cihaz elektrik kesintisi
// sonrası aynı anda açılırsa hepsi aynı saniyelerde dener ve AP'yi gereksiz
// yükler. Jitter denemeleri dağıtır.
//
// ── HESAPLAR SAF ────────────────────────────────────────────────────────────
// `esp_random()` burada ÇAĞRILMAZ; rastgele bayt parametre olarak gelir.
// Böylece TASK-064 backoff eğrisini ve jitter sınırlarını deterministik
// test edebilir.

#include <stdint.h>

#include "core/Time.h"
#include "services/network/NetworkEvents.h"

namespace services {
namespace net {
namespace retry {

constexpr uint32_t BASE_DELAY_MS   = 1000u;
constexpr uint32_t MAX_DELAY_MS    = 20000u;  ///< tavan — gerekçe yukarıda
constexpr uint32_t FAST_RETRY_MS   = 500u;    ///< LINK_LOST ilk denemesi
constexpr uint8_t  MAX_AUTH_TRIES  = 3u;      ///< kimlik hatasında üst sınır
constexpr uint32_t STABLE_MS       = 30000u;  ///< sayacı sıfırlamak için gereken süre
constexpr uint8_t  JITTER_PERCENT  = 20u;

/// Üstel taban gecikme — jitter'sız, tavanlı.
///
/// `attempt` 0'dan başlar. Kaydırma 6 ile sınırlı: `1000 << 6 = 64000` zaten
/// tavanı aşar, daha fazla kaydırma taşma riski üretir. (Tavan 20 sn'ye
/// indikten sonra sınır 5. denemede zaten devreye girer; kaydırma sınırı
/// TAŞMA koruması olarak duruyor.)
constexpr uint32_t baseDelayMs(uint8_t attempt)
{
    return ((attempt >= 6u) ? MAX_DELAY_MS : (BASE_DELAY_MS << attempt)) > MAX_DELAY_MS
               ? MAX_DELAY_MS
               : ((attempt >= 6u) ? MAX_DELAY_MS : (BASE_DELAY_MS << attempt));
}

/// Jitter uygular: `base` ± %20. Sonuç TAVANI AŞMAZ ve tabanın altına inmez.
///
/// `rnd` çağıranın sağladığı 0–255 arası bir bayttır.
constexpr uint32_t applyJitter(uint32_t base, uint8_t rnd)
{
    // sapma = base * (rnd/255 * 2*%20 - %20)  →  [-%20, +%20]
    return (base + (base * static_cast<uint32_t>(rnd) * 2u * JITTER_PERCENT) / (255u * 100u) -
            (base * JITTER_PERCENT) / 100u) > MAX_DELAY_MS
               ? MAX_DELAY_MS
               : (base + (base * static_cast<uint32_t>(rnd) * 2u * JITTER_PERCENT) /
                             (255u * 100u) -
                  (base * JITTER_PERCENT) / 100u);
}

/// Bir sonraki denemeye kadar beklenecek süre.
///
/// `LINK_LOST` ilk denemesi HIZLIDIR: geçici sinyal kaybı çoğu zaman hemen
/// düzelir ve 1 saniye beklemek gereksiz kesinti üretir.
constexpr uint32_t delayFor(DisconnectClass c, uint8_t attempt, uint8_t rnd)
{
    return (c == DisconnectClass::LINK_LOST && attempt == 0u)
               ? FAST_RETRY_MS
               : applyJitter(baseDelayMs(attempt), rnd);
}

/// Denemeyi tamamen durdurmalı mıyız?
///
/// Yalnızca kimlik doğrulama hatasında ve yalnızca `MAX_AUTH_TRIES` sonrası.
/// Şifre yanlışsa 1000. deneme de başarısız olur; kullanıcıya bildirmek
/// denemeye devam etmekten faydalıdır.
constexpr bool shouldStop(DisconnectClass c, uint8_t authFailures)
{
    return c == DisconnectClass::AUTH_FAILED && authFailures >= MAX_AUTH_TRIES;
}

// --- Derleme zamanı doğrulama ----------------------------------------------

static_assert(baseDelayMs(0) == 1000u, "ilk gecikme 1 sn");
static_assert(baseDelayMs(1) == 2000u, "ikinci gecikme 2 sn");
static_assert(baseDelayMs(4) == 16000u, "besinci gecikme 16 sn");
static_assert(baseDelayMs(5) == MAX_DELAY_MS, "altinci denemede tavana oturur");
static_assert(baseDelayMs(6) == MAX_DELAY_MS, "tavan 20 sn");
static_assert(baseDelayMs(200) == MAX_DELAY_MS, "buyuk deneme sayisinda TASMA YOK");

// Jitter sınırları: taban 10 sn için [8 sn, 12 sn] aralığında kalmalı.
static_assert(applyJitter(10000u, 0u) == 8000u, "jitter alt sinir -%20");
static_assert(applyJitter(10000u, 255u) == 12000u, "jitter ust sinir +%20");
static_assert(applyJitter(MAX_DELAY_MS, 255u) == MAX_DELAY_MS, "jitter TAVANI ASAMAZ");

// LINK_LOST ilk denemesi hızlı, ikincisi normal backoff.
static_assert(delayFor(DisconnectClass::LINK_LOST, 0u, 128u) == FAST_RETRY_MS,
              "sinyal kopmasinda ilk deneme hizli olmali");
static_assert(delayFor(DisconnectClass::LINK_LOST, 1u, 0u) == 1600u,
              "ikinci deneme normal backoff'a doner");
// Kimlik hatası ASLA hızlı denemez — sonsuz döngü üretirdi.
static_assert(delayFor(DisconnectClass::AUTH_FAILED, 0u, 128u) != FAST_RETRY_MS,
              "kimlik hatasinda hizli deneme YASAK");

static_assert(!shouldStop(DisconnectClass::AUTH_FAILED, 2u), "3 denemeden once durmaz");
static_assert(shouldStop(DisconnectClass::AUTH_FAILED, 3u), "3 denemede durur");
static_assert(!shouldStop(DisconnectClass::AP_NOT_FOUND, 99u),
              "AP bulunamadi durumunda ASLA durmaz - AP geri gelebilir");

} // namespace retry
} // namespace net
} // namespace services
