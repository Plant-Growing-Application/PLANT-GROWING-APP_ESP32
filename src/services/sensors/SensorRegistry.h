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

/// Kayıtlı sensör sayısı. `MAX_SENSORS` (8) üst sınırdır ve TASK-066 ile
/// TAMAMI kullanıldı: nem sensörü nihayet gerçek bir sürücüye kavuştu
/// (AHT20), yanına ortam sıcaklığı ve ışık eklendi.
///
/// Dokuzuncu bir sensör `MAX_SENSORS`'ı büyütmeyi gerektirir; bu, `Config`
/// NVS blob'unu 24 bayt büyütür ve boyut `static_assert`'i uyarır.
constexpr uint8_t REGISTERED_SENSOR_COUNT = 8;

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

    // --- I2C ortam sensörleri (TASK-066) ---
    //
    // Ortam sıcaklığı ve nem AYNI ÇİPTEN (AHT20) gelir ve ardışık sıralanır:
    // ikisi de `hal::aht20::service()` çağırır, sürücü aynı turdaki ikinci
    // çağrıyı yok sayar. Ayrık periyot vermek çipi gereksiz yere iki kat
    // sık tetiklerdi.
    //
    // 5 sn: hava sıcaklığı ve nem bu ölçekte ölçülebilir biçimde değişmez;
    // daha sık okumak I2C hattını OLED ile gereksiz yere paylaştırır.
    {core::SensorId::AMBIENT_TEMP, SensorUnit::CELSIUS, 0, 0, core::millisecs(5000)},
    {core::SensorId::HUMIDITY, SensorUnit::PERCENT, 0, 0, core::millisecs(5000)},

    // Işık daha hızlı değişir (bulut, perde, lambanın açılması) ve büyütme
    // ışığının gerçekten yandığını doğrulamak için kullanılır.
    {core::SensorId::LIGHT, SensorUnit::LUX, 0, 0, core::millisecs(2000)},
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
