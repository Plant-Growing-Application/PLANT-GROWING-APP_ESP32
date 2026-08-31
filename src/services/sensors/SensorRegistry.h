#pragma once

// Sensör kayıt tablosu — TASK-022
//
// DERLEME ZAMANI SABİT TABLO: dinamik kayıt, fabrika deseni ve heap yok
// (ARCHITECTURE §9.1).
//
// Yeni sensör eklemek = tabloya bir satır + bir `ISensor` implementasyonu.
//
// Örnekleme periyotları ARCHITECTURE §9.3 kataloğundan gelir. Güvenlik
// zincirinin girdisi olan sensörler `isSafetyCritical = 1` işaretlidir;
// `SensorService` (TASK-027) bunları HİÇBİR döngüde atlamaz.

#include <stdint.h>

#include "ISensor.h"
#include "core/SystemState.h"
#include "core/Time.h"

namespace services {
namespace sensors {

/// Kayıtlı sensör sayısı. `MAX_SENSORS` (8) üst sınırdır; şu an 5 sensör
/// tanımlı — nem sensörü donanımda yok (REQUIREMENTS §3.5), eklendiğinde
/// tabloya bir satır olarak girer.
constexpr uint8_t REGISTERED_SENSOR_COUNT = 5;

/// Sensör tanımlayıcıları — ARCHITECTURE §9.3 kataloğu.
constexpr SensorDescriptor kSensorTable[REGISTERED_SENSOR_COUNT] = {
    // Su seviyesi: GÜVENLİK ZİNCİRİNİN TEMELİ. En sık örneklenen sensör;
    // pompa çalışırken seviye hızla düşebilir.
    {core::SensorId::WATER_LEVEL, SensorUnit::LEVEL_STATE, 1, 0, core::millisecs(500)},

    // Akış: kuru çalışma tespitinin girdisi (TASK-031).
    {core::SensorId::WATER_FLOW, SensorUnit::LITERS_PER_MIN, 1, 0, core::millisecs(1000)},

    // Su sıcaklığı: bilgi amaçlı, ama EC telafisi buna bağımlı.
    {core::SensorId::WATER_TEMP, SensorUnit::CELSIUS, 0, 0, core::millisecs(1000)},

    // pH ve EC: bilgi + otomasyon eşiği. Problar DC beslemede polarize olur,
    // bu yüzden daha seyrek örneklenir.
    {core::SensorId::PH, SensorUnit::PH, 0, 0, core::millisecs(2000)},
    {core::SensorId::EC, SensorUnit::MILLISIEMENS, 0, 0, core::millisecs(2000)},
};

/// Kimliğe göre tanımlayıcı bulur; yoksa `nullptr`.
inline const SensorDescriptor* describe(core::SensorId id)
{
    for (uint8_t i = 0; i < REGISTERED_SENSOR_COUNT; ++i)
    {
        if (kSensorTable[i].id == id)
        {
            return &kSensorTable[i];
        }
    }
    return nullptr;
}

// --- Derleme zamanı doğrulama ----------------------------------------------

static_assert(REGISTERED_SENSOR_COUNT <= core::MAX_SENSORS,
              "Kayitli sensor sayisi MAX_SENSORS'i asamaz");

// Güvenlik sensörleri en sık örneklenmeli: seviye 500 ms, akış 1 sn.
// Bu değerler gevşetilirse güvenlik tepkisi yavaşlar.
static_assert(kSensorTable[0].id == core::SensorId::WATER_LEVEL,
              "Su seviyesi tablonun ILK sirasinda olmali (guvenlik onceligi)");
static_assert(kSensorTable[0].isSafetyCritical == 1,
              "Su seviyesi guvenlik-kritik isaretlenmeli");
static_assert(kSensorTable[1].isSafetyCritical == 1,
              "Akis sensoru guvenlik-kritik isaretlenmeli");
static_assert(kSensorTable[0].samplePeriod.ms <= 500,
              "Su seviyesi en az 2 Hz orneklenmeli");

} // namespace sensors
} // namespace services
