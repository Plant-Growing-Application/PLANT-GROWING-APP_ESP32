#pragma once

// Güvenlik denetleyicisi — TASK-030
//
// SİSTEMİN EN KRİTİK MODÜLÜ. ARCHITECTURE §12.2'nin dört ilkesi burada
// pazarlıksız uygulanır:
//
//   1. FAIL-SAFE VARSAYILAN  — okunamayan durum = tehlikeli durum kabul edilir
//   2. TEK VETO NOKTASI      — tüm açma yolları buradan geçer, yan kapı yok
//   3. BAĞIMSIZLIK           — MANUAL modda da tam yetkili, otomasyondan bağımsız
//   4. GÖZLEMLENEBİLİRLİK    — her veto neden koduyla loglanır
//
// ── SAFTIR ──────────────────────────────────────────────────────────────────
// Donanıma dokunmaz, röleye uzanmaz, `services/` çağırmaz. Girdisi bir
// `SystemState` snapshot'ı + `Config` + aşım sayacı; çıktısı bir kilit
// maskesidir. Röleyi kapatma işini `ActuatorManager` yapar — SafetyMonitor
// yalnızca "hayır" der. Bu ayrım sayesinde TASK-064 tüm kilit
// kombinasyonlarını donanımsız koşturabilir.
//
// ── DEĞERLENDİRME SIKLIĞI ───────────────────────────────────────────────────
// Her `app_core` döngüsünde (100 ms) KOŞULSUZ tam hesaplama. Değişim
// tetiklemeli bir tasarımda kaçırılan tek bir tetik, kilidin hiç
// hesaplanmaması demektir — ve bu SESSİZ bir arızadır.

#include <stdint.h>

#include "core/Config.h"
#include "core/ErrorCodes.h"
#include "core/SystemState.h"
#include "core/Time.h"
#include "domain/models/SafetyState.h"

namespace domain {
namespace safety {

/// Denetleyiciyi başlatır.
core::ErrCode begin(const core::Config& cfg);

/// Kilitleri yeniden hesaplar. **Otomasyondan ÖNCE çağrılmalıdır** (§11.1
/// adım 3). Bu sıra değiştirilemez: otomasyon güvenliğin izin verdiği alanda
/// karar verir, tersi değil.
///
/// @param snap  o döngünün `StateStore` snapshot'ı
/// @param now   monotonik zaman
void evaluate(const core::SystemState& snap, core::Millis now);

/// Bir aktüatör ŞU AN enerjili olabilir mi?
///
/// `OK` = izin var; başka bir kod = engelin nedeni. `ActuatorManager`'a
/// `SafetyPermitFn` olarak verilir; hem açma hem devam etme kararında
/// kullanılır (TASK-030 Karar 2).
core::ErrCode permits(core::ActuatorId id);

/// Aktif kilit maskesi (`Interlock` bitleri).
uint32_t interlocks();

/// Dışarıdan gelen kilitleri ayarlar — kaynağı bu modül değildir.
///
/// `ILK_DRY_RUN` TASK-031'in, `ILK_EMERGENCY_LATCHED` TASK-032'nin
/// kararıdır. SafetyMonitor bunları hesaplamaz, yalnızca veto zincirine
/// dahil eder; böylece **tek veto noktası** ilkesi korunur.
void setExternalInterlock(Interlock bit, bool active);

/// **Operatör onaylı kurtarma** — mandalların TEK temizleme noktası.
///
/// Mandallar birbirine bağlıdır: kuru çalışma mandalı temizlenmeden acil
/// durum onayı sonsuza dek reddedilirdi (kuru çalışma kilidi hâlâ aktif
/// görünür). Ayrı ayrı temizleme yolları bırakmak bu tuzağı üretir.
///
/// Sıra:
///   1. CANLI fiziksel koşulları kontrol et (su seviyesi) — düzelmemişse
///      **reddet**. Operatör önce hazneyi doldurmalıdır.
///   2. Kuru çalışma mandalını temizle
///   3. Acil durum mandalını temizle (aşım sayaçları dahil)
///   4. Kilitleri yeniden hesapla
///
/// Adım 1'de yalnızca **canlı** koşullar bakılır; mandallar ve sayaçlar
/// zaten temizlenecek olanlardır — onları kontrol etmek kendini engelleyen
/// bir kilit yaratırdı.
///
/// @return `OK` = temizlendi; `SAFETY_BLOCKED` = fiziksel koşullar düzelmemiş
core::ErrCode acknowledge(const core::SystemState& snap, core::Millis now);

/// `safety` alt-state'ini `StateStore`'a yayınlar.
core::ErrCode publish(core::Millis now);

} // namespace safety
} // namespace domain
