#pragma once

// Sensör arayüzü ve tanımlayıcısı — TASK-022
//
// SIĞ SOYUTLAMA (ARCHITECTURE §9.1): tek arayüz + derleme zamanı tablo.
// YASAK: derin sınıf hiyerarşisi, fabrika deseni, dinamik kayıt, heap üzerinde
// sensör nesneleri, ikiden fazla sanal fonksiyon.
//
// Gerekçe: dört sensör aynı analog hattı paylaşır (kod tekrarı olmasın diye
// soyutlama gerekli), ama sensör sayısı sabit ve az (derin soyutlama gereksiz).
//
// TİP SAHİPLİĞİ (ISSUE-010): `SensorQuality` ve `SensorSample` `SystemState.h`
// içinde TANIMLI (yayınlanan state). Burada YENİDEN TANIMLANMAZ, include edilir.

#include <stdint.h>

#include "core/Config.h"
#include "core/ErrorCodes.h"
#include "core/SystemState.h"

namespace services {
namespace sensors {

/// Ölçüm birimi. Sensör başına SABİT olduğu için `SensorSample`'da değil,
/// `SensorDescriptor`'da taşınır — her örnekte tekrarlamak state'i büyütürdü.
enum class SensorUnit : uint8_t
{
    NONE          = 0,
    CELSIUS       = 1,
    PH            = 2,
    MILLISIEMENS  = 3,  ///< EC
    LITERS_PER_MIN = 4,
    PERCENT       = 5,
    LEVEL_STATE   = 6,  ///< ayrık seviye kodu (bkz. WaterLevelSensor)
};

/// Sensörün değişmeyen özellikleri — derleme zamanında sabittir.
struct SensorDescriptor
{
    core::SensorId id;
    SensorUnit     unit;
    uint8_t        isSafetyCritical;  ///< 1 = güvenlik zincirinin girdisi
    uint8_t        reserved;
    core::Duration samplePeriod;      ///< bu sensör ne sıklıkla okunmalı
};

/// Sensörler arası bağlam.
///
/// NEDEN VAR: EC ölçümü sıcaklığa güçlü bağımlıdır (sıcaklık telafisi), ama
/// sensörler birbirini ÇAĞIRAMAZ — eşit seviyededirler ve birbirlerine
/// bağımlı olmaları döngüsel bağımlılık yaratır.
///
/// `SensorService` o an bilinen değerleri bu yapıda taşır. Bağımlılık AÇIK ve
/// TEK YÖNLÜDÜR: EC sensörü sıcaklığı istemez, verilen bağlamı kullanır.
/// Sıcaklık geçersizse EC bunu bilir ve kendi kalitesini düşürür.
struct SampleContext
{
    core::Millis now;
    float        waterTempC;
    bool         waterTempValid;
};

/// Sensörün ürettiği HAM (ama fiziksel birime çevrilmiş) ölçüm.
///
/// Sensör "bu 23.4 °C" der. "Bu değer güvenilir mi" sorusunu işleme hattı
/// (TASK-023) yanıtlar.
struct RawSample
{
    float value;

    /// Donanım seviyesinde bir sorun görüldü mü? (ADC uçta sabit, I2C yanıt
    /// vermiyor, darbe sayacı okunamadı...)
    bool hardwareFault;

    /// Ölçüm fiziksel olarak şüpheli mi? Örneğin ADC uçta sabit: değer
    /// hesaplanabildi ama kopuk/kısa devre ihtimali var.
    bool suspect;

    /// Sensöre özgü güven düşüşü — örneğin EC için sıcaklık telafisi
    /// yapılamadıysa. Hat bunu kaliteye yansıtır.
    bool lowConfidence;
};

constexpr RawSample rawOk(float v)
{
    return RawSample{v, false, false, false};
}

constexpr RawSample rawFault()
{
    return RawSample{0.0f, true, false, false};
}

/// Minimal sensör arayüzü — **tam olarak iki sanal fonksiyon**.
class ISensor
{
public:
    virtual ~ISensor() = default;

    /// Donanımı hazırlar. Config bu sensörün kalibrasyon ve aralık ayarlarını
    /// taşır.
    virtual core::ErrCode begin(const core::SensorConfig& cfg) = 0;

    /// Bir ölçüm alır ve **fiziksel birime çevirir**.
    ///
    /// Sensöre özgü matematik (NTC Beta denklemi, pH eğrisi, darbe→L/dk)
    /// BURADA yapılır. Genel `offset`/`scale` trim, filtre ve doğrulama
    /// işleme hattının (TASK-023) işidir.
    virtual RawSample sample(const SampleContext& ctx) = 0;
};

} // namespace sensors
} // namespace services
