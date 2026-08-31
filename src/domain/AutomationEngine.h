#pragma once

// Otomasyon motoru — TASK-057
//
// ── M4 KAPISI: BU MODÜL KAPALI DOĞAR ────────────────────────────────────────
// IMPLEMENTATION_PLAN: "M4 doğrulanmadan PHASE 12 (otomasyon) başlatılmaz."
// M4 donanımda kanıtlanmadı (ISSUE-018). Kod derleniyor ama:
//   · `AutomationConfig.mode` varsayılanı `MANUAL`
//   · varsayılan kural kümesi TAMAMEN BOŞ
//   · `MANUAL` modda kurallar HİÇ değerlendirilmez
// Yani hiçbir aktüatör kendiliğinden çalışmaz.
//
// ── OTOMASYONUN BİLMEDİĞİ ŞEYLER (§11.4) ────────────────────────────────────
// Motor yalnızca "şu aktüatörün açık olmasını istiyorum" der. Bilmez:
//   · güvenlik kilitleri      → `SafetyMonitor`  (TASK-030)
//   · aktüatör kısıtları      → `ActuatorManager` (TASK-029)
//   · röle polaritesi, pin    → `BoardPins`
//
// `request()` sonucunu KAYDEDER ama davranışını DEĞİŞTİRMEZ: kural bir
// sonraki döngüde aynı isteği yine üretir ve kısıt/kilit kalkınca
// kendiliğinden uygulanır. Motorun "cooldown var, istemeyeyim" demesi,
// kısıt mantığının iki yere dağılması olurdu.
//
// ── SÜRELİ MANUEL OVERRIDE (§10.3) ──────────────────────────────────────────
// AUTO modda operatör bir aktüatöre müdahale ederse, otomasyon O AKTÜATÖR
// için `manualOverrideMs` süresince susar. Kalıcı override, unutulan bir
// komutun otomasyonu süresiz devre dışı bırakması demektir — hidroponikte
// bitki kaybıdır.
//
// Override AKTÜATÖR BAŞINADIR: hava pompasına müdahale, su pompasının
// otomasyonunu durdurmaz.

#include <stdint.h>

#include "core/Config.h"
#include "core/ErrorCodes.h"
#include "core/SystemState.h"
#include "core/Time.h"

namespace domain {
namespace automation {

core::ErrCode begin(const core::Config& cfg);

/// Bir değerlendirme çevrimi. **`app_core` içinde, güvenlik
/// değerlendirmesinden SONRA** çağrılır (§11.1 adım 4) — bu sıra
/// TASK-033'te sabitlenmiştir.
///
/// @param snap       o döngünün snapshot'ı
/// @param timeValid  saat geçerli mi (çizelgeler buna bağlı)
void evaluate(const core::SystemState& snap, bool timeValid, core::Millis now);

/// Operatörün manuel müdahalesini bildirir — o aktüatör için otomasyonu
/// `manualOverrideMs` süresince susturur.
///
/// `app_core` bir MANUAL kaynaklı `SET_ACTUATOR` komutunu uyguladığında
/// çağırır.
void noteManualCommand(core::ActuatorId id, core::Millis now);

/// Modu değiştirir ve config'e yazar (kalıcı).
core::ErrCode setMode(core::AutomationMode mode);

core::AutomationMode mode();

/// `automation` alt-state'ini yayınlar.
core::ErrCode publish(const core::SystemState& snap, bool timeValid, core::Millis now);

} // namespace automation
} // namespace domain
