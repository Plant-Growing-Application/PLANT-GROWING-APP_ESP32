#include "WaterLevelSensor.h"

#include <Arduino.h>

#include "core/BoardPins.h"

namespace services {
namespace sensors {

core::ErrCode WaterLevelSensor::begin(const core::SensorConfig& cfg)
{
    (void)cfg;  // eşikler ayrık; kalibrasyon bu sensöre uygulanmaz

    _low.pin  = board::LEVEL_FLOAT_LOW;
    _crit.pin = board::LEVEL_FLOAT_CRIT;

    // Dahili pull-up ZORUNLU: kontak açıldığında (veya kablo koptuğunda)
    // giriş HIGH'a çekilmeli ki "su yok" okunsun (fail-safe kablolama,
    // docs/HARDWARE.md §9).
    pinMode(_low.pin, INPUT_PULLUP);
    pinMode(_crit.pin, INPUT_PULLUP);

    const uint32_t now = millis();

    // FAIL-SAFE BAŞLANGIÇ: ilk okuma yapılana kadar "su yok" varsayılır.
    // Ters varsayım, boot ile ilk örnek arasında pompanın çalışmasına
    // izin verirdi.
    _low.stable        = false;
    _low.lastRaw       = false;
    _low.lastChangeMs  = now;
    _crit.stable       = false;
    _crit.lastRaw      = false;
    _crit.lastChangeMs = now;

    _state        = WaterLevelState::CRITICAL;
    _inconsistent = false;
    _ready        = true;

    return core::ErrCode::OK;
}

void WaterLevelSensor::updateSwitch(FloatSwitch& sw, uint32_t nowMs)
{
    // Kablolama (docs/HARDWARE.md §9): normalde kapalı kontak, pull-up'lı giriş.
    //   Su VAR      → kontak KAPALI → LOW
    //   Su YOK      → kontak AÇIK   → HIGH
    //   KABLO KOPUK →                HIGH → "su yok"  ✓ fail-safe
    const bool raw = (digitalRead(sw.pin) == LOW);

    if (raw != sw.lastRaw)
    {
        sw.lastRaw      = raw;
        sw.lastChangeMs = nowMs;
        return;  // debounce penceresi başladı
    }

    if ((nowMs - sw.lastChangeMs) >= DEBOUNCE_MS)
    {
        sw.stable = raw;
    }
}

RawSample WaterLevelSensor::sample(const SampleContext& ctx)
{
    if (!_ready)
    {
        _state        = WaterLevelState::CRITICAL;  // fail-safe
        _inconsistent = false;
        return rawFault();
    }

    const uint32_t nowMs = ctx.now.v;
    updateSwitch(_low, nowMs);
    updateSwitch(_crit, nowMs);

    const bool waterAtLow  = _low.stable;   // üst şamandıra: su var mı
    const bool waterAtCrit = _crit.stable;  // alt şamandıra: su var mı

    // -----------------------------------------------------------------------
    // TUTARLILIK KONTROLÜ
    //
    // Üst şamandıra suda yüzerken alttakinin kuru olması FİZİKSEL OLARAK
    // İMKÂNSIZDIR. Bu kombinasyon en az bir sensörün arızalı olduğunu gösterir;
    // hangisi olduğunu bilemediğimiz için HER İKİSİ DE arızalı sayılır.
    //
    // Sonuç FAULT → `SafetyMonitor` pompayı kilitler (fail-safe).
    // -----------------------------------------------------------------------
    if (waterAtLow && !waterAtCrit)
    {
        _inconsistent = true;
        _state        = WaterLevelState::CRITICAL;
        return rawFault();
    }

    _inconsistent = false;

    if (waterAtLow && waterAtCrit)
    {
        _state = WaterLevelState::SUFFICIENT;
    }
    else if (!waterAtLow && waterAtCrit)
    {
        _state = WaterLevelState::LOW_LEVEL;
    }
    else
    {
        // İkisi de kuru → kritik seviye.
        // Kablo koptuğunda da bu dala düşülür — kasıtlı ve doğru.
        _state = WaterLevelState::CRITICAL;
    }

    return rawOk(levelToValue(_state));
}

} // namespace sensors
} // namespace services
