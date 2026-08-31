#pragma once

// Sensör işleme hattı — TASK-023
//
//   sample → calibrate → filter → validate → quality → publish
//
// SAF FONKSİYON: girdi ham örnek + config + filtre durumu + zaman; çıktı
// işlenmiş örnek. Donanım yok, log yok, global yok.
//
// Bu sayede TASK-064 host tarafında SENTETİK VERİYLE test edebilir: gürültülü
// sinyal, tek seferlik sıçrama, donmuş değer, aralık dışı — hepsi donanımsız.
//
// GÖREV BÖLÜMÜ (TASK-022 Karar 4):
//   Sensör (TASK-024/025/026) → "bu 23.4 °C"      (sensöre özgü matematik)
//   Hat    (bu dosya)          → "bu değer güvenilir mi"

#include <stdint.h>

#include "ISensor.h"
#include "core/Config.h"
#include "core/SystemState.h"
#include "core/Time.h"

namespace services {
namespace sensors {

/// Bir sensörün hat durumu — filtre belleği ve teşhis sayaçları.
///
/// Sensör başına bir tane; `SensorService` sabit dizide tutar.
struct PipelineState
{
    float        filtered;       ///< EMA çıktısı
    float        lastRawValue;   ///< değişim hızı ve bayatlama için
    core::Millis lastSampleAt;
    core::Millis lastChangeAt;   ///< değerin en son ANLAMLI değiştiği an
    uint16_t     warmupCount;    ///< filtre "ısınana" kadar sayılan örnek
    uint16_t     faultCount;     ///< biriken hata (teşhis, API'de görünür)
    uint8_t      initialized;
    uint8_t      reserved[3];

    void reset()
    {
        filtered     = 0.0f;
        lastRawValue = 0.0f;
        lastSampleAt = core::Millis{0};
        lastChangeAt = core::Millis{0};
        warmupCount  = 0;
        faultCount   = 0;
        initialized  = 0;
    }
};

namespace pipeline {

/// Bir ham örneği işleyip yayınlanabilir bir `SensorSample` üretir.
///
/// SAF: `st` dışında hiçbir şeyi değiştirmez, hiçbir yan etki üretmez.
///
/// @param id   sensör kimliği (çıktıya kopyalanır)
/// @param raw  sensörün ürettiği fiziksel değer + donanım bayrakları
/// @param cfg  bu sensörün kalibrasyon/aralık/filtre ayarları
/// @param st   hat durumu — **değiştirilir**
/// @param now  monotonik zaman
core::SensorSample process(core::SensorId id, const RawSample& raw,
                           const core::SensorConfig& cfg, PipelineState& st,
                           core::Millis now);

/// Devre dışı bırakılmış sensör için örnek üretir.
///
/// `enabled == 0` → donanım HİÇ OKUNMAZ; doğrudan `NOT_PRESENT`.
/// Arayüz bunu "arıza" değil "takılı değil" olarak gösterir.
core::SensorSample notPresent(core::SensorId id, core::Millis now);

// --- Ayarlanabilir eşikler --------------------------------------------------

/// Bir değerin "değişmedi" sayılması için gereken maksimum fark.
/// ADC gürültü seviyesinin altında olmalı ki sabit bir ortamdaki gerçek
/// ölçüm yanlışlıkla `STALE` sayılmasın.
constexpr float STALE_EPSILON = 0.001f;

/// Değer bu süre boyunca hiç değişmezse `STALE`.
constexpr core::Duration STALE_TIMEOUT = core::seconds(60);

} // namespace pipeline
} // namespace sensors
} // namespace services
