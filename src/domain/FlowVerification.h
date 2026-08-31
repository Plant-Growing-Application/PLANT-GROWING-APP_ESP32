#pragma once

// Akış doğrulama ve kuru çalışma koruması — TASK-031
//
// Hidroponik bir sistemde POMPA KAYBI BİTKİ KAYBIDIR. Bu modül pompanın
// gerçekten su bastığını akış sensörüyle doğrular.
//
// ── MANTIK ──────────────────────────────────────────────────────────────────
//   pompa KAPALI            → doğrulama yok (akış olmaması normaldir)
//   pompa AÇILDI            → `flowVerifyDelayMs` boyunca bekle (boru dolması)
//   gecikme doldu, akış OK  → normal
//   gecikme doldu, akış YOK → KURU ÇALIŞMA → mandalla, pompayı kilitle
//
// ── İKİ AYRI ARIZA, TEK SONUÇ ───────────────────────────────────────────────
//   sensör kalitesi bozuk → SAFETY_FLOW_VERIFY_FAILED  (koruma çalışmıyor)
//   kalite OK, debi düşük → SAFETY_DRY_RUN             (gerçek kuru çalışma)
//
// İkisi de pompayı durdurur — "koruma çalışmıyorsa korunan şey
// çalıştırılmaz". Ancak kodları AYRIDIR: biri tesisat, diğeri kablo
// sorunudur ve müdahaleleri farklıdır.
//
// ── MANDAL ──────────────────────────────────────────────────────────────────
// Arıza kendiliğinden temizlenmez. Aralıklı kuru çalışma pompayı YAVAŞÇA
// öldürür; mandal olmasaydı sistem "çalış → kuru → dur → soğu → çalış"
// döngüsüne girer, her turda biraz daha hasar verir ve kimse fark etmezdi.

#include <stdint.h>

#include "core/Config.h"
#include "core/ErrorCodes.h"
#include "core/SystemState.h"
#include "core/Time.h"

namespace domain {
namespace flow {

/// Doğrulama durumu — tanılama ve arayüz için.
enum class VerifyPhase : uint8_t
{
    INACTIVE = 0,  ///< pompa kapalı — doğrulama yapılmıyor
    PRIMING  = 1,  ///< pompa açık, gecikme dolmadı (boru doluyor)
    VERIFIED = 2,  ///< akış doğrulandı
    FAILED   = 3,  ///< arıza mandallandı
};

core::ErrCode begin(const core::Config& cfg);

/// Doğrulamayı ilerletir. **`safety::evaluate()` tarafından çağrılır** —
/// app_core doğrudan çağırmaz; tek çağrı noktası unutulma riskini kaldırır.
///
/// @param snap  o döngünün snapshot'ı (akış örneği için)
/// @param now   monotonik zaman — modül `millis()` çağırmaz, host'ta
///              hızlandırılmış test edilebilir
void evaluate(const core::SystemState& snap, core::Millis now);

/// Mandal aktif mi?
bool latched();

/// Mandallanan arızanın nedeni (`SAFETY_DRY_RUN` veya
/// `SAFETY_FLOW_VERIFY_FAILED`); mandal yoksa `OK`.
core::ErrCode reason();

VerifyPhase phase();

/// Mandalı temizler — **operatör onayı** yolundan (TASK-032).
void acknowledge();

} // namespace flow
} // namespace domain
