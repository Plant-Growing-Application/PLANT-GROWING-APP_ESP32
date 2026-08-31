#pragma once

// Güvenlik kilidi modeli — TASK-030
//
// Kilitler BİTMASK'tır: aynı anda birden fazla kilit aktif olabilir ve
// operatör hepsini görmelidir. "Pompa çalışmıyor" mesajı tek bir nedene
// indirgenirse, ikinci nedeni giderilmeden birinci neden düzeltilir ve
// kullanıcı sistemi bozuk sanır (ARCHITECTURE §12.2 gözlemlenebilirlik).
//
// MAKRO ÇAKIŞMA TARAMASI (ISSUE-009): `ILK_` öneki bilinçlidir. Arduino.h ve
// ESP-IDF başlıklarında `INTERLOCK`, `LATCHED`, `DRY_RUN`, `EMERGENCY`
// makroları taranmış, çakışma bulunmamıştır; önek yine de gelecekteki
// framework güncellemelerine karşı yalıtım sağlar.

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/SystemState.h"

namespace domain {
namespace safety {

/// Kilit bitleri. `SafetyStatus.interlockMask` içinde yayınlanır.
enum Interlock : uint32_t
{
    ILK_NONE                = 0u,
    ILK_EMERGENCY_LATCHED   = 1u << 0,  ///< acil durum mandalı aktif (TASK-032)
    ILK_LEVEL_INSUFFICIENT  = 1u << 1,  ///< su seviyesi LOW veya CRITICAL
    ILK_LEVEL_SENSOR_FAULT  = 1u << 2,  ///< seviye okunamıyor → en kötü durum varsayılır
    ILK_DRY_RUN             = 1u << 3,  ///< akış doğrulama başarısız (TASK-031)
    ILK_MAX_RUNTIME_REPEATED = 1u << 4, ///< maxRunMs tekrarlı aşıldı → sistemik arıza
};

/// Bir kilidin neden kodu — loglama ve arayüz için.
constexpr core::ErrCode reasonOf(Interlock bit)
{
    return (bit == ILK_EMERGENCY_LATCHED)   ? core::ErrCode::SAFETY_EMERGENCY_LATCHED
         : (bit == ILK_LEVEL_INSUFFICIENT)  ? core::ErrCode::SAFETY_LEVEL_INSUFFICIENT
         : (bit == ILK_LEVEL_SENSOR_FAULT)  ? core::ErrCode::SAFETY_LEVEL_SENSOR_FAULT
         : (bit == ILK_DRY_RUN)             ? core::ErrCode::SAFETY_DRY_RUN
         : (bit == ILK_MAX_RUNTIME_REPEATED)? core::ErrCode::ACTUATOR_MAX_RUNTIME
                                            : core::ErrCode::OK;
}

/// Kilit → aktüatör eşlemesi (TASK-030 Karar 3).
///
/// Hava pompası seviye kilitlerinden ETKİLENMEZ: hava taşı susuz kalınca
/// hasar görmez, yalnızca işe yaramaz. Kuru çalışma riski su pompasına
/// özgüdür. Gereksiz kilit, operatörün güvenlik uyarılarına
/// duyarsızlaşmasına yol açar.
constexpr uint32_t masksFor(core::ActuatorId id)
{
    return (id == core::ActuatorId::WATER_PUMP)
               ? (ILK_EMERGENCY_LATCHED | ILK_LEVEL_INSUFFICIENT | ILK_LEVEL_SENSOR_FAULT |
                  ILK_DRY_RUN | ILK_MAX_RUNTIME_REPEATED)
               : (ILK_EMERGENCY_LATCHED | ILK_MAX_RUNTIME_REPEATED);
}

/// Maskede aktif olan İLK kilidin neden kodu.
///
/// Sıra önem sırasıdır: acil durum > seviye > kuru çalışma > süre aşımı.
/// Bir aktüatör birden çok nedenle engelliyse operatöre en ciddi olan
/// gösterilir; tamamı `interlockMask` üzerinden zaten görülebilir.
constexpr core::ErrCode firstReason(uint32_t mask)
{
    return (mask & ILK_EMERGENCY_LATCHED)    ? reasonOf(ILK_EMERGENCY_LATCHED)
         : (mask & ILK_LEVEL_SENSOR_FAULT)   ? reasonOf(ILK_LEVEL_SENSOR_FAULT)
         : (mask & ILK_LEVEL_INSUFFICIENT)   ? reasonOf(ILK_LEVEL_INSUFFICIENT)
         : (mask & ILK_DRY_RUN)              ? reasonOf(ILK_DRY_RUN)
         : (mask & ILK_MAX_RUNTIME_REPEATED) ? reasonOf(ILK_MAX_RUNTIME_REPEATED)
                                             : core::ErrCode::OK;
}

// --- Derleme zamanı doğrulama ----------------------------------------------

// Su pompası TÜM kilitlerden etkilenir; hava pompası seviyeden etkilenmez.
static_assert((masksFor(core::ActuatorId::WATER_PUMP) & ILK_LEVEL_INSUFFICIENT) != 0u,
              "su pompasi seviye kilidinden ETKILENMELI");
static_assert((masksFor(core::ActuatorId::AIR_PUMP) & ILK_LEVEL_INSUFFICIENT) == 0u,
              "hava pompasi seviye kilidinden ETKILENMEMELI");
// Acil durum her aktüatörü keser — istisnasız.
static_assert((masksFor(core::ActuatorId::WATER_PUMP) & ILK_EMERGENCY_LATCHED) != 0u,
              "acil durum su pompasini kesmeli");
static_assert((masksFor(core::ActuatorId::AIR_PUMP) & ILK_EMERGENCY_LATCHED) != 0u,
              "acil durum hava pompasini kesmeli");
static_assert((masksFor(core::ActuatorId::AUX_1) & ILK_EMERGENCY_LATCHED) != 0u,
              "acil durum yedek cikislari da kesmeli");
// Acil durum önceliği: aynı anda seviye kilidi varken bile acil durum raporlanır.
static_assert(firstReason(ILK_EMERGENCY_LATCHED | ILK_LEVEL_INSUFFICIENT) ==
                  core::ErrCode::SAFETY_EMERGENCY_LATCHED,
              "acil durum en yuksek oncelikli neden olmali");

} // namespace safety
} // namespace domain
