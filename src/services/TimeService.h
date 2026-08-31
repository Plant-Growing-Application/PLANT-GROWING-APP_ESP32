#pragma once

// Zaman servisi — TASK-040
//
// ── GEÇERSİZ ZAMANDA SAHTE DEĞER YOK ────────────────────────────────────────
// Eski sistemde `getFormattedTime()` senkronize değilken sessizce
// `"00:00:00"` döndürüyordu. Bir çizelge bunu "gece yarısı" sanar ve sulama
// yapar. Burada geçersiz zaman `EPOCH_INVALID` olarak döner ve çağıran
// geçerliliği sormak zorundadır.
//
// ── §11.2 ZAMAN GEÇERLİLİĞİ KURALI ──────────────────────────────────────────
//   `valid == false` → ÇİZELGE kuralları çalışmaz
//                    → EŞİK kuralları çalışmaya DEVAM EDER
//                    → UI ve web "saat geçersiz" gösterir
//
// ── SÜRE ÖLÇÜMLERİ DUVAR SAATİYLE YAPILMAZ ──────────────────────────────────
// SNTP saati ileri veya geri alabilir. `maxRunTime`, cooldown, backoff ve
// akış doğrulama monotonik `Millis` kullanır. Bu ayrım `core/Time.h`'ta üç
// ayrı tiple derleme zamanında zaten zorunlu kılınmıştır.
//
// ── DONANIMSAL RTC (ISSUE-005) ──────────────────────────────────────────────
// DS3231 **ertelendi, reddedilmedi**: satın alınmamış bir yonga için sürücü
// yazmak P7 ihlalidir. İşlevsel sonuç açıkça kabul ediliyor — güç kesintisi
// + ağ yoksa çizelgeler çalışmaz, eşik kuralları çalışır. Eklenmesi
// gerekirse değişiklik bu dosyada kalır.

#include <stdint.h>

#include "core/Config.h"
#include "core/ErrorCodes.h"
#include "core/Time.h"

namespace services {
namespace timesvc {

/// Kristal kayması için saatte bir yeterli; dakikada bir israf.
constexpr uint32_t RESYNC_PERIOD_MS = 3600000u;

/// İlk senkronizasyon denemeleri arasındaki süre.
constexpr uint32_t FIRST_SYNC_RETRY_MS = 15000u;

core::ErrCode begin(const core::Config& cfg);

/// Bir çevrim. `netConnected` yanlışsa SNTP denenmez — ağ yokken deneme
/// radyo ve CPU harcar, sonucu bellidir.
void tick(core::Millis now, bool netConnected);

/// Zaman geçerli mi? Çizelgeler bunu sormak zorundadır.
bool valid();

/// Duvar saati. Geçersizken `EPOCH_INVALID`.
core::EpochSeconds epoch();

/// İlk senkronizasyonun ne kadar sürdüğü (ms). Henüz olmadıysa 0.
uint32_t firstSyncMs();

/// `time` alt-state'ini yayınlar.
core::ErrCode publish(core::Millis now);

/// Timezone değişti — POSIX TZ yeniden uygulanır.
void applyTimezone();

} // namespace timesvc
} // namespace services
