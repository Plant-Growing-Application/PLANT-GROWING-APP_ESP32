#pragma once

// Akış sensörü — TASK-025
//
// GÜVENLİK ROLÜ: kuru çalışma tespitinin (TASK-031) girdisidir.
//
// ── MEVCUT ALGORİTMA REDDEDİLDİ ────────────────────────────────────────────
//   Eski: litersPerMinute = (pulses * 100) / 450
//
//   1. SABİT zaman penceresi varsayıyordu — fonksiyon 500 ms ve 600 ms
//      periyotlarla çağrılıyordu, sonuç anlamsızdı
//   2. TAMSAYI bölmesi düşük debileri SIFIRA yuvarlıyordu — kuru çalışma
//      tespiti için en kritik bölge tam da orası
//   3. İki farklı çağıran aynı sayacı tüketiyordu
//
//   Yeni: debi = (darbe / darbePerLitre) / (geçenSüre_ms / 60000) → float L/dk
//   PCNT gerçek geçen süreyi sayımla BİRLİKTE veriyor (TASK-019).
// ────────────────────────────────────────────────────────────────────────────
//
// SIFIR AKIŞ ≠ SENSÖR ARIZASI: "0 L/dk" hem "pompa kapalı, normal" hem
// "sensör kopuk, kuru çalışma" olabilir. Ayrım POMPA DURUMU gerektirir ve
// bu sensör onu bilmez — çapraz kontrol TASK-031'in işidir.

#include <stdint.h>

#include "ISensor.h"

namespace services {
namespace sensors {

namespace flow {

/// Sensör modelinin fiziksel özelliği: litre başına darbe.
///
/// !!! DOĞRULANMAMIŞ !!! Bu değer eski kodun yorumundan geliyor (YF-S401,
/// ~450 darbe/L) ve ölçülmedi. Gerçek sensörle bilinen hacim akıtılarak
/// doğrulanmalıdır (ISSUE-014). Saha trim'i `SensorConfig.scale` ile yapılır.
constexpr float PULSES_PER_LITER = 450.0f;

/// Donanımsal gürültü filtresi (ns). Sensörün minimum darbe genişliğinden
/// kısa olmalı; çok agresif değer GERÇEK darbeleri de eler.
constexpr uint16_t GLITCH_FILTER_NS = 1000;

} // namespace flow

class FlowSensor : public ISensor
{
public:
    core::ErrCode begin(const core::SensorConfig& cfg) override;
    RawSample     sample(const SampleContext& ctx) override;

    /// Açılıştan beri toplam akan hacim (litre).
    ///
    /// RAM'de tutulur. Kalıcılık TASK-058 (geçmiş veri) kapsamında
    /// değerlendirilecek — orası zaten periyodik yazma yapıyor; ikinci bir
    /// kalıcılık mekanizması kurmak gereksiz flash aşınması olurdu.
    float totalLiters() const { return _totalLiters; }

private:
    float _totalLiters = 0.0f;
    bool  _ready       = false;
};

} // namespace sensors
} // namespace services
