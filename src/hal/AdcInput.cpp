#include "AdcInput.h"

#include <driver/adc.h>
#include <esp_adc_cal.h>

#include "core/BoardPins.h"
#include "core/Diagnostics.h"

namespace hal {
namespace adc {
namespace {

using core::ErrCode;

/// Zayıflatma: 11 dB → yaklaşık 0–3.1 V ölçüm aralığı.
/// Sensörler 3.3 V beslemeli olduğu için tam aralık gerekir.
constexpr adc_atten_t ATTEN      = ADC_ATTEN_DB_12;  // ~0–3.1 V (DB_11 kullanimdan kaldirildi)
constexpr adc_bits_width_t WIDTH = ADC_WIDTH_BIT_12;
constexpr uint32_t DEFAULT_VREF  = 1100;  ///< eFuse yoksa nominal değer

esp_adc_cal_characteristics_t g_cal{};
bool                          g_hasFactoryCal = false;
bool                          g_ready         = false;

/// GPIO → ADC1 kanalı. ADC1 DIŞINDAKİ her pin için `-1` döner.
int adc1ChannelOf(uint8_t pin)
{
    switch (pin)
    {
    case 36: return ADC1_CHANNEL_0;
    case 37: return ADC1_CHANNEL_1;
    case 38: return ADC1_CHANNEL_2;
    case 39: return ADC1_CHANNEL_3;
    case 32: return ADC1_CHANNEL_4;
    case 33: return ADC1_CHANNEL_5;
    case 34: return ADC1_CHANNEL_6;
    case 35: return ADC1_CHANNEL_7;
    default: return -1;
    }
}

} // namespace

core::ErrCode begin()
{
    if (g_ready)
    {
        return ErrCode::OK;
    }

    adc1_config_width(WIDTH);

    // Fabrika kalibrasyonu kartlar arası tutarlılık sağlar. Yoksa nominal
    // eğriye düşülür — sessizce değil, loglanarak.
    const esp_adc_cal_value_t src =
        esp_adc_cal_characterize(ADC_UNIT_1, ATTEN, WIDTH, DEFAULT_VREF, &g_cal);

    g_hasFactoryCal = (src != ESP_ADC_CAL_VAL_DEFAULT_VREF);

    if (!g_hasFactoryCal)
    {
        core::diag::log(core::LogLevel::WARNING, ErrCode::OK, static_cast<int32_t>(src),
                        "ADC fabrika kalibrasyonu yok — nominal egri kullaniliyor");
    }

    g_ready = true;
    return ErrCode::OK;
}

core::ErrCode configurePin(uint8_t pin)
{
    const int ch = adc1ChannelOf(pin);
    if (ch < 0)
    {
        // ADC2 pini veya ADC olmayan pin. Wi-Fi aktifken ADC2 okunamadığı için
        // bu sessizce kabul edilmez — açıkça reddedilir.
        core::diag::log(core::LogLevel::ERROR, ErrCode::CFG_VALIDATION_FAILED,
                        static_cast<int32_t>(pin), "pin ADC1 degil — reddedildi");
        return ErrCode::CFG_VALIDATION_FAILED;
    }
    adc1_config_channel_atten(static_cast<adc1_channel_t>(ch), ATTEN);
    return ErrCode::OK;
}

core::Result<AdcSample> read(uint8_t pin)
{
    if (!g_ready)
    {
        return core::err<AdcSample>(ErrCode::SYS_BOOT_STAGE_FAILED);
    }

    const int ch = adc1ChannelOf(pin);
    if (ch < 0)
    {
        return core::err<AdcSample>(ErrCode::CFG_VALIDATION_FAILED);
    }

    // Çoklu örnekleme: ESP32 ADC'si gürültülüdür, tek okuma yetersizdir.
    uint32_t sum = 0;
    for (uint8_t i = 0; i < SAMPLES_PER_READ; ++i)
    {
        sum += static_cast<uint32_t>(adc1_get_raw(static_cast<adc1_channel_t>(ch)));
    }
    const uint16_t raw = static_cast<uint16_t>(sum / SAMPLES_PER_READ);

    AdcSample s;
    s.raw        = raw;
    s.millivolts = static_cast<uint16_t>(esp_adc_cal_raw_to_voltage(raw, &g_cal));

    // Uç değer TESPİTİ sürücüde; KARAR üst katmanda (D6).
    // Uçta sabit bir değer kopuk veya kısa devre göstergesidir.
    s.atRail = (raw == 0u) || (raw >= ADC_MAX_RAW);

    return core::ok(s);
}

bool hasFactoryCalibration()
{
    return g_hasFactoryCal;
}

} // namespace adc
} // namespace hal
