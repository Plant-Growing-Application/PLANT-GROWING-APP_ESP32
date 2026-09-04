#include "EnvSensors.h"

#include "hal/Aht20.h"
#include "hal/Bh1750.h"

namespace services {
namespace sensors {

// ---------------------------------------------------------------------------
// AmbientTempSensor
// ---------------------------------------------------------------------------

core::ErrCode AmbientTempSensor::begin(const core::SensorConfig& cfg)
{
    (void)cfg;  // kalibrasyon trim'i hat katmanında uygulanır

    // Çip iki sensör tarafından paylaşılıyor; `hal::aht20::begin()` idempotent
    // değildir ama iki kez çağrılması zararsızdır: durum makinesi sıfırlanır
    // ve ilk ölçüm yeniden tetiklenir. Sıra bağımlılığı YOKTUR.
    const core::ErrCode rc = hal::aht20::begin();
    _ready = (rc == core::ErrCode::OK);
    return rc;
}

RawSample AmbientTempSensor::sample(const SampleContext& ctx)
{
    if (!_ready || !hal::aht20::isAvailable())
    {
        return rawFault();
    }

    hal::aht20::service(ctx.now);

    if (!hal::aht20::hasReading())
    {
        // Henüz tamamlanmış ölçüm yok. Bu, boot'tan hemen sonraki ilk turda
        // veya arka arkaya CRC hatasından sonra olabilir; ikisi de "değer
        // güvenilmez" demektir ve hat bunu FAULT'a çevirir.
        return rawFault();
    }

    return rawOk(hal::aht20::temperatureC());
}

// ---------------------------------------------------------------------------
// HumiditySensor
// ---------------------------------------------------------------------------

core::ErrCode HumiditySensor::begin(const core::SensorConfig& cfg)
{
    (void)cfg;

    const core::ErrCode rc = hal::aht20::begin();
    _ready = (rc == core::ErrCode::OK);
    return rc;
}

RawSample HumiditySensor::sample(const SampleContext& ctx)
{
    if (!_ready || !hal::aht20::isAvailable())
    {
        return rawFault();
    }

    hal::aht20::service(ctx.now);

    if (!hal::aht20::hasReading())
    {
        return rawFault();
    }

    return rawOk(hal::aht20::humidityPct());
}

// ---------------------------------------------------------------------------
// LightSensor
// ---------------------------------------------------------------------------

core::ErrCode LightSensor::begin(const core::SensorConfig& cfg)
{
    (void)cfg;

    const core::ErrCode rc = hal::bh1750::begin();
    _ready = (rc == core::ErrCode::OK);
    return rc;
}

RawSample LightSensor::sample(const SampleContext& ctx)
{
    (void)ctx;  // BH1750 sürekli moddadır; zaman damgası gerekmez

    if (!_ready || !hal::bh1750::isAvailable())
    {
        return rawFault();
    }

    float lux = 0.0f;
    if (!hal::bh1750::read(lux))
    {
        return rawFault();
    }

    return rawOk(lux);
}

} // namespace sensors
} // namespace services
