#pragma once

// Analog giriş sürücüsü — TASK-018
//
// ── KESİN KISIT: YALNIZCA ADC1 ──────────────────────────────────────────────
// Wi-Fi radyosu aktifken **ADC2 KULLANILAMAZ**. Bu bir tercih değil, donanım
// kısıtıdır: ADC2 Wi-Fi tarafından paylaşılır ve okuma başarısız olur.
//
// Bu sürücü ADC2 pinlerini KABUL ETMEZ; `BoardPins.h`'daki `isAdc1()` kontrolü
// derleme zamanında, buradaki kontrol çalışma zamanında zorlar.
// ────────────────────────────────────────────────────────────────────────────
//
// SÜRÜCÜDE SENSÖRE ÖZGÜ FORMÜL YOKTUR (D6). NTC/pH/EC dönüşümleri TASK-024'te.
// Bu sürücü ham değer ve gerilim üretir; anlamlandırmak `services/`'in işidir.

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/Result.h"

namespace hal {

/// Tek bir ADC okumasının sonucu.
struct AdcSample
{
    uint16_t raw;        ///< ham ADC değeri (12 bit: 0–4095)
    uint16_t millivolts; ///< kalibre edilmiş gerilim
    bool     atRail;     ///< uçta sabit (0 veya tam ölçek) — kopuk/kısa göstergesi
};

namespace adc {

/// ADC1'i yapılandırır ve varsa **fabrika kalibrasyon verisini** (eFuse) yükler.
///
/// eFuse kalibrasyonu kartlar arası tutarlılık sağlar; yoksa nominal değerlere
/// düşülür ve bu durum loglanır.
core::ErrCode begin();

/// Bir ADC1 pinini kullanıma hazırlar.
///
/// @return CFG_VALIDATION_FAILED — pin ADC1 değilse (Wi-Fi + ADC2 çakışması)
core::ErrCode configurePin(uint8_t pin);

/// Çoklu örnekleme ile okur.
///
/// ESP32 ADC'si gürültülü ve doğrusal değildir; tek okuma yetersizdir.
/// Örnek sayısı `SAMPLES_PER_READ` ile sabittir ve `io_sense` task'ının
/// 250 ms bütçesine göre seçilmiştir.
///
/// UÇ DEĞER TESPİTİ sürücüde yapılır (`atRail`) ama KARAR üst katmana aittir:
/// sürücü "bu değer uçta" der, "sensör arızalı" demez (D6).
core::Result<AdcSample> read(uint8_t pin);

/// Fabrika kalibrasyonu (eFuse) mevcut mu? Yoksa ölçümler nominal eğriye
/// dayanır ve kartlar arasında sapma gösterebilir.
bool hasFactoryCalibration();

/// Okuma başına örnek sayısı. Gürültü bastırma ile süre arasındaki denge;
/// TASK-062'de ölçümle gözden geçirilecek.
constexpr uint8_t SAMPLES_PER_READ = 16;

/// 12 bit ADC üst değeri.
constexpr uint16_t ADC_MAX_RAW = 4095;

} // namespace adc
} // namespace hal
