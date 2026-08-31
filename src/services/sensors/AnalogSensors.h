#pragma once

// Analog sensörler: su sıcaklığı (NTC), pH, EC — TASK-024
//
// Bu dosyadaki her sınıf HAM ADC OKUMASINI FİZİKSEL BİRİME çevirir.
// Genel `offset`/`scale` trim, filtre ve doğrulama işleme hattının işidir
// (TASK-023) — sensöre özgü matematik burada, genel işleme orada.
//
// ── MATEMATİKSEL DOMAIN HATALARI ───────────────────────────────────────────
// Mevcut sistemin NTC formülü şuydu:
//
//   temperature = 1.0/(log((4095.0/sensorValue) - 1.0)/3950.0 + 1.0/298.15) - 273.15
//
// Üç hatası vardı ve sonucu sessizce kullanılıyordu (REQUIREMENTS §3.1):
//   1. `sensorValue == 0`    → 4095/0 = inf → log(inf) tanımsız
//   2. `sensorValue == 4095` → log(0) = -inf
//   3. Seri direnç ve besleme gerilimi hesaba katılmamış
//
// Buradaki formüllerde her bölme ve `log()` domain hatası AÇIKÇA korunur;
// hesaplanamayan değer `hardwareFault` ile FAULT'a dönüşür.
// ────────────────────────────────────────────────────────────────────────────

#include <stdint.h>

#include "ISensor.h"

namespace services {
namespace sensors {

// ---------------------------------------------------------------------------
// Su sıcaklığı — NTC termistör
//
// Devre parametreleri DERLEME ZAMANI SABİTİDİR: bunlar fiziksel devrenin
// özellikleridir. `SensorConfig.offset`/`scale` ise saha trim'i içindir —
// biri devrenin ne olduğunu, diğeri ölçümün ne kadar kaydığını söyler.
// (Röle polaritesiyle aynı gerekçe, TASK-017 Karar 1.)
// ---------------------------------------------------------------------------
namespace ntc {

/// Bölücüdeki sabit direnç (ohm).
constexpr float R_SERIES = 10000.0f;

/// NTC'nin 25 °C'deki direnci (ohm).
constexpr float R_NOMINAL = 10000.0f;

/// Beta katsayısı (K) — sensör veri sayfasından.
constexpr float BETA = 3950.0f;

/// Nominal sıcaklık, Kelvin.
constexpr float T_NOMINAL_K = 298.15f;

/// Besleme gerilimi (mV).
constexpr float VCC_MV = 3300.0f;

} // namespace ntc

/// NTC tabanlı su sıcaklığı sensörü.
///
/// BÖLÜCÜ TOPOLOJİSİ VARSAYIMI (doğrulanmalı — docs/HARDWARE.md):
///   NTC → VCC, R_SERIES → GND  ⇒ sıcaklık artınca V_out ARTAR
/// Ters bağlıysa okumalar ters yönde değişir ve ilk denemede fark edilir.
class WaterTempSensor : public ISensor
{
public:
    core::ErrCode begin(const core::SensorConfig& cfg) override;
    RawSample     sample(const SampleContext& ctx) override;

private:
    bool _ready = false;
};

// ---------------------------------------------------------------------------
// pH
//
// 2 nokta (pH 4.0 / pH 7.0) doğrusal kalibrasyon standarttır ve eğim/ofset
// ikilisine indirgenebilir — bu ikisi `SensorConfig.scale`/`offset` üzerinden
// uygulanır (işleme hattında).
//
// Burada yalnızca gerilim → nominal pH dönüşümü yapılır.
// ---------------------------------------------------------------------------
namespace ph {

/// pH 7.0'da beklenen prob çıkışı (mV) — tipik modüllerde besleme ortası.
constexpr float MV_AT_PH7 = 1650.0f;

/// pH birimi başına gerilim değişimi (mV/pH). Nernst eğimi ~59.16 mV/pH'tir;
/// yaygın modüller bunu yükseltir. Saha kalibrasyonu `scale` ile düzeltir.
constexpr float MV_PER_PH = -177.0f;

} // namespace ph

class PhSensor : public ISensor
{
public:
    core::ErrCode begin(const core::SensorConfig& cfg) override;
    RawSample     sample(const SampleContext& ctx) override;

private:
    bool _ready = false;
};

// ---------------------------------------------------------------------------
// EC (elektriksel iletkenlik)
//
// SICAKLIK TELAFİSİ: EC ölçümü sıcaklığa güçlü bağımlıdır (~%2/°C).
//     EC_25 = EC_ölçülen / (1 + 0.02 × (T − 25))
//
// Sıcaklık geçersizse telafi YAPILAMAZ. Değer yine de yayınlanır ama
// `lowConfidence` işaretlenir: kullanıcı değeri görür, otomasyon ona
// güvenmez (hat kaliteyi STALE'e düşürür).
// ---------------------------------------------------------------------------
namespace ec {

/// mV → mS/cm nominal dönüşüm katsayısı. Modele göre değişir; saha
/// kalibrasyonu `scale` ile düzeltir.
constexpr float MS_PER_MV = 0.00195f;

/// Sıcaklık telafi katsayısı (1/°C).
constexpr float TEMP_COEFF = 0.02f;

/// Telafinin referans sıcaklığı.
constexpr float REF_TEMP_C = 25.0f;

} // namespace ec

class EcSensor : public ISensor
{
public:
    core::ErrCode begin(const core::SensorConfig& cfg) override;
    RawSample     sample(const SampleContext& ctx) override;

private:
    bool _ready = false;
};

} // namespace sensors
} // namespace services
