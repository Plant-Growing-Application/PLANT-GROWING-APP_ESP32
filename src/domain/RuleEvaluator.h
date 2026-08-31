#pragma once

// Kural değerlendirme — TASK-055 (eşik) / TASK-056 (çizelge)
//
// ── SAF ─────────────────────────────────────────────────────────────────────
// Girdi: kural + kural durumu + ölçüm/zaman. Çıktı: `RuleVerdict`.
// Donanım yok, global yok, log yok. Tek yan etki `RuleRuntime`'ın referansla
// güncellenmesidir — çağıranın sahip olduğu bellek üzerinde.
//
// TASK-064 histerezis sınırlarını, suspect zamanlayıcısını ve sarma
// pencereleri **hızlandırılmış ve deterministik** koşturabilir.
//
// ── HİSTEREZİS NEDEN ZORUNLU ────────────────────────────────────────────────
//   Histerezissiz: eşik 1.0. Ölçüm 0.99 → AÇ. Ölçüm 1.01 → KAPAT.
//                  Gürültü nedeniyle saniyede birkaç kez çevrim; röle ve
//                  pompa hızla yıpranır.
//   Histerezisli:  AÇ 0.9, KAPAT 1.1. Bir kez açıldıktan sonra 1.1'e
//                  ulaşana kadar açık kalır.
//
// ── ZAMAN GEÇERLİLİĞİ (ARCHITECTURE §11.2) ──────────────────────────────────
//   saat geçersiz → ÇİZELGE kuralları değerlendirilmez
//                 → EŞİK kuralları çalışmaya devam eder

#include <stdint.h>

#include "core/Rule.h"
#include "core/SystemState.h"
#include "core/Time.h"

namespace domain {
namespace rules {

/// Sensör kalitesi bozulduğunda mevcut durumun korunacağı süre.
///
/// Anlık bir sensör hatası (tek bozuk okuma, geçici gürültü) sistemi
/// durdurmamalı; kalıcı hata güvenli tarafa düşürmeli (TASK-055 Karar 1).
constexpr uint32_t SUSPECT_GRACE_MS = 30000u;

/// Bir gün = 1440 dakika.
constexpr uint16_t MINUTES_PER_DAY = 1440u;

// --- Saf yardımcılar (host'ta test edilebilir) ------------------------------

/// Sarma pencereler dahil, verilen dakika pencerenin içinde mi?
///
/// `22:00–02:00` bir hata DEĞİL, sarma penceredir ve gece sulamasında
/// yaygındır. Bu, çizelge mantığındaki en yaygın hata kaynağıdır.
constexpr bool inWindow(uint16_t nowMin, uint16_t startMin, uint16_t endMin)
{
    return (startMin < endMin) ? (nowMin >= startMin && nowMin < endMin)
                               : (nowMin >= startMin || nowMin < endMin);
}

/// Periyodik çevrimde şu an açık mı?
///
/// Referans DUVAR SAATİDİR (gün içi saniye), uptime değil: uptime tabanlı
/// bir çevrim her reset'te sıfırlanır ve günde birkaç reset yaşayan bir
/// sistem aşırı sular. Duvar saati tabanlı hesap reset'e dayanıklıdır,
/// öngörülebilirdir ve flash yazması gerektirmez.
constexpr bool inCycle(uint32_t secondOfDay, uint16_t onS, uint16_t periodS)
{
    return (periodS == 0u) ? false : ((secondOfDay % periodS) < onS);
}

// --- Değerlendiriciler ------------------------------------------------------

/// Eşik kuralını değerlendirir — TASK-055.
///
/// @param sample  ilgili sensörün örneği; `nullptr` = sensör hiç yok
core::RuleVerdict evaluateThreshold(const core::Rule& r, core::RuleRuntime& rt,
                                    const core::SensorSample* sample, core::Millis now);

/// Çizelge kuralını değerlendirir — TASK-056.
///
/// @param timeValid  `false` ise kural DEĞERLENDİRİLMEZ ve `applies == 0`
/// @param epoch      duvar saati (yerel zamana çevrilir)
core::RuleVerdict evaluateSchedule(const core::Rule& r, core::RuleRuntime& rt,
                                   bool timeValid, core::EpochSeconds epoch,
                                   core::Millis now);

/// Bir sonraki çizelge tetiklemesinin duvar saati — arayüzde gösterilir.
/// Hesaplanamıyorsa `EPOCH_INVALID`.
core::EpochSeconds nextScheduleAt(const core::RuleSet& rs, bool timeValid,
                                  core::EpochSeconds epoch);

// --- Derleme zamanı doğrulama ----------------------------------------------

// Normal pencere
static_assert(inWindow(600, 480, 720), "10:00, 08:00-12:00 icinde olmali");
static_assert(!inWindow(780, 480, 720), "13:00, 08:00-12:00 disinda olmali");
static_assert(inWindow(480, 480, 720), "baslangic DAHIL");
static_assert(!inWindow(720, 480, 720), "bitis HARIC");

// SARMA pencere (22:00–02:00) — en yaygın hata kaynağı
static_assert(inWindow(1380, 1320, 120), "23:00, 22:00-02:00 icinde");
static_assert(inWindow(0, 1320, 120), "00:00, 22:00-02:00 icinde");
static_assert(inWindow(60, 1320, 120), "01:00, 22:00-02:00 icinde");
static_assert(!inWindow(720, 1320, 120), "12:00, 22:00-02:00 DISINDA");
static_assert(!inWindow(120, 1320, 120), "02:00 bitis HARIC");

// Çevrim: 2 saatte 15 dk → periyot 7200 sn, açık 900 sn
static_assert(inCycle(0, 900, 7200), "cevrim basi ACIK");
static_assert(inCycle(899, 900, 7200), "899. sn hala ACIK");
static_assert(!inCycle(900, 900, 7200), "900. sn KAPALI");
static_assert(inCycle(7200, 900, 7200), "sonraki cevrim basi ACIK");
static_assert(!inCycle(7199, 900, 7200), "cevrim sonu KAPALI");
static_assert(!inCycle(0, 900, 0), "periyot 0 -> ASLA acilmaz (sifira bolme yok)");

} // namespace rules
} // namespace domain
