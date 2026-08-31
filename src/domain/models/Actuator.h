#pragma once

// Aktüatör çalışma modeli ve kısıtları — TASK-028
//
// TİP SAHİPLİĞİ (ISSUE-010): `ActuatorId`, `ControlSource` ve `ActuatorStatus`
// `SystemState.h`'ta (yayınlanan state), `CommandResult` `Command.h`'ta
// (TASK-008), `ActuatorConfig` `Config.h`'ta (TASK-014) TANIMLI.
// Burada YALNIZCA çalışma tipleri var.
//
// KISIT FONKSİYONLARI SAFTIR: girdi config + runtime + zaman, çıktı karar.
// Yan etki, donanım, log yok. Bu sayede TASK-064 min/max/cooldown sınırlarını
// SINIR DEĞERLERİNDE host tarafında test edebilir — bir pompa kısıtını sahada
// denemek pahalı ve yavaştır.
//
// ZAMAN: tüm ölçümler monotonik (`Millis` + `hasElapsed`). NTP duvar saatini
// geriye alabilir; duvar saatiyle yapılan bir "pompa 3 saattir çalışıyor"
// hesabı `maxRunMs` korumasını sessizce bozardı (CODING_STANDARDS Z4/Z5).

#include <stdint.h>
#include <type_traits>

#include "core/Command.h"
#include "core/Config.h"
#include "core/SystemState.h"
#include "core/Time.h"

namespace domain {

/// Bir aktüatörün iç çalışma durumu — YAYINLANMAZ.
///
/// Yayınlanan özet (`ActuatorStatus`) bundan türetilir. İç zamanlayıcıların
/// arayüze sızması gereksizdir ve `SystemState` 312 baytta kalmalıdır.
struct ActuatorRuntime
{
    core::Millis lastOnAt;        ///< röle en son ne zaman açıldı
    core::Millis lastOffAt;       ///< röle en son ne zaman kapandı
    uint32_t     totalRunMs;      ///< toplam çalışma süresi (bakım göstergesi)
    uint16_t     cycleCount;      ///< açılma sayısı
    uint16_t     maxRunViolations;///< maxRunMs aşımı sayısı → SafetyMonitor izler
    core::ErrCode blockReason;    ///< açılamıyorsa nedeni; OK = engel yok
    core::ControlSource source;   ///< mevcut durumu kim belirledi
    uint8_t      desired;         ///< TALEP edilen durum — kısıt nedeniyle henüz uygulanmamış olabilir
    uint8_t      isOn;            ///< yazılımın bildiği durum (gerçek pin ayrı okunur)
    uint8_t      everRan;         ///< hiç çalıştı mı (cooldown ilk açılışta uygulanmasın)
    uint8_t      reserved;

    void reset()
    {
        lastOnAt         = core::Millis{0};
        lastOffAt        = core::Millis{0};
        totalRunMs       = 0;
        cycleCount       = 0;
        maxRunViolations = 0;
        blockReason      = core::ErrCode::OK;
        source           = core::ControlSource::NONE;
        desired          = 0;
        isOn             = 0;
        everRan          = 0;
    }
};

/// Kısıt değerlendirmesinin sonucu.
///
/// `CommandResult`'a doğrudan eşlenir; ayrı bir sonuç enum'u tanımlanmadı (P7).
struct ConstraintVerdict
{
    bool             allowed;
    core::ErrCode    reason;   ///< engelin nedeni (OK = engel yok)
    core::CommandResult result; ///< çağırana dönecek sonuç
};

constexpr ConstraintVerdict verdictAllow()
{
    return ConstraintVerdict{true, core::ErrCode::OK, core::CommandResult::ACCEPTED};
}

constexpr ConstraintVerdict verdictBlock(core::ErrCode reason, core::CommandResult r)
{
    return ConstraintVerdict{false, reason, r};
}

namespace actuator {

/// Aktüatör AÇILABİLİR mi? (Yalnızca kısıtlar — güvenlik kilidi AYRI sorulur.)
///
/// `cooldownMs`: kapandıktan sonra bu süre dolmadan tekrar açılamaz. Pompa
/// ömrünü korur (kısa çevrim aşınması).
///
/// İlk açılışta cooldown UYGULANMAZ: `everRan == 0` iken `lastOffAt` anlamsızdır
/// ve sıfır kabul edilirse cihaz boot'tan sonra cooldown süresi kadar
/// gereksiz beklerdi.
constexpr ConstraintVerdict canTurnOn(const core::ActuatorConfig& cfg,
                                      const ActuatorRuntime& rt, core::Millis now)
{
    return (rt.everRan != 0u && cfg.cooldownMs > 0u &&
            !core::hasElapsed(now, rt.lastOffAt, core::Duration{cfg.cooldownMs}))
               ? verdictBlock(core::ErrCode::ACTUATOR_COOLDOWN,
                              core::CommandResult::DEFERRED_COOLDOWN)
               : verdictAllow();
}

/// Aktüatör KAPATILABİLİR mi?
///
/// `minRunMs`: açıldıktan sonra en az bu kadar çalışmalı (kısa çevrim koruması).
/// Bu kısıt ACİL DURUMDA UYGULANMAZ — `forceOff` yolu kısıt tanımaz.
constexpr ConstraintVerdict canTurnOff(const core::ActuatorConfig& cfg,
                                       const ActuatorRuntime& rt, core::Millis now)
{
    return (rt.isOn != 0u && cfg.minRunMs > 0u &&
            !core::hasElapsed(now, rt.lastOnAt, core::Duration{cfg.minRunMs}))
               ? verdictBlock(core::ErrCode::ACTUATOR_MIN_RUNTIME,
                              core::CommandResult::DEFERRED_MIN_RUNTIME)
               : verdictAllow();
}

/// `maxRunMs` aşıldı mı? Aşıldıysa aktüatör ZORLA kapatılmalıdır.
///
/// `maxRuntimeGraceMs` küçük gecikmeleri tolere eder; asıl koruma budur:
/// sınırsız çalışan bir pompa hazneyi boşaltır ve kuru çalışır.
constexpr bool maxRunExceeded(const core::ActuatorConfig& cfg, const ActuatorRuntime& rt,
                              core::Millis now, uint32_t graceMs)
{
    return rt.isOn != 0u &&
           core::hasElapsed(now, rt.lastOnAt, core::Duration{cfg.maxRunMs + graceMs});
}

/// Kaynak tahkimi — ARCHITECTURE §10.3.
///
///   SAFETY (3) > MANUAL (2) > AUTOMATION (1) > NONE (0)
///
/// Yeni kaynak, mevcut durumu belirleyen kaynağı geçersiz kılabilir mi?
constexpr bool sourceOutranks(core::ControlSource incoming, core::ControlSource current)
{
    return static_cast<uint8_t>(incoming) >= static_cast<uint8_t>(current);
}

} // namespace actuator

// --- Derleme zamanı doğrulama ----------------------------------------------

static_assert(std::is_trivially_copyable<ActuatorRuntime>::value,
              "ActuatorRuntime POD olmali");
static_assert(sizeof(ActuatorRuntime) <= 32, "ActuatorRuntime 32 bayti asmamali");

// Tahkim sırası ARCHITECTURE §10.3 ile uyumlu mu?
static_assert(actuator::sourceOutranks(core::ControlSource::SAFETY,
                                       core::ControlSource::MANUAL),
              "SAFETY, MANUAL'i gecersiz kilabilmeli");
static_assert(actuator::sourceOutranks(core::ControlSource::MANUAL,
                                       core::ControlSource::AUTOMATION),
              "MANUAL, AUTOMATION'i gecersiz kilabilmeli");
static_assert(!actuator::sourceOutranks(core::ControlSource::AUTOMATION,
                                        core::ControlSource::MANUAL),
              "AUTOMATION, MANUAL'i gecersiz KILAMAMALI");

} // namespace domain
