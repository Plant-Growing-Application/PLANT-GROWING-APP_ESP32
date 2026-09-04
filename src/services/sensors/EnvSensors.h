#pragma once

// Ortam sensörleri: hava sıcaklığı, bağıl nem, ışık — TASK-066
//
// Bu dosya `hal::aht20` ve `hal::bh1750` çip sürücülerini `ISensor` arayüzüne
// bağlar. Görev bölümü TASK-022 Karar 4'teki gibidir:
//
//   hal/         → çipin protokolü, CRC, durum makinesi
//   bu dosya     → "bu 23.4 °C" (fiziksel birime çevrilmiş ham değer)
//   pipeline     → "bu değer güvenilir mi"
//
// ── TEK ÇİP, İKİ SENSÖR ─────────────────────────────────────────────────────
// `AmbientTempSensor` ve `HumiditySensor` AYNI AHT20'yi paylaşır. İkisi de
// `hal::aht20::service()` çağırır; sürücü aynı turdaki ikinci çağrıyı yok
// sayar. Ayrı sürücü örneği yaratmak iki ayrı ölçüm tetiklemesi demek olurdu
// ve çip hiçbirini tamamlayamazdı.
//
// ── GÜVENLİK ROLÜ YOK ───────────────────────────────────────────────────────
// Üçü de `isSafetyCritical = 0`'dır. Bir ortam sensörünün arızası pompayı
// kilitlemez; yalnızca ona bağlı otomasyon kuralı (ısıtıcı, ışık) çalışmaz —
// bu, `RuleEvaluator`'ın sensör kalitesi denetimiyle zaten sağlanır.

#include <stdint.h>

#include "ISensor.h"

namespace services {
namespace sensors {

/// Ortam (hava) sıcaklığı — AHT20.
class AmbientTempSensor : public ISensor
{
public:
    core::ErrCode begin(const core::SensorConfig& cfg) override;
    RawSample     sample(const SampleContext& ctx) override;

private:
    bool _ready = false;
};

/// Bağıl nem — AHT20.
///
/// `SensorId::HUMIDITY`'nin İLK gerçek sürücüsü: kimlik TASK-006'dan beri
/// tanımlıydı ama onu okuyan hiçbir kod yoktu.
class HumiditySensor : public ISensor
{
public:
    core::ErrCode begin(const core::SensorConfig& cfg) override;
    RawSample     sample(const SampleContext& ctx) override;

private:
    bool _ready = false;
};

/// Aydınlık düzeyi (lüks) — BH1750.
class LightSensor : public ISensor
{
public:
    core::ErrCode begin(const core::SensorConfig& cfg) override;
    RawSample     sample(const SampleContext& ctx) override;

private:
    bool _ready = false;
};

} // namespace sensors
} // namespace services
