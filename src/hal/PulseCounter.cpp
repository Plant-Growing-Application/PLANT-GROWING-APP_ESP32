#include "PulseCounter.h"

#include <Arduino.h>
#include <driver/pcnt.h>

#include "core/Diagnostics.h"

namespace hal {
namespace pulse {
namespace {

using core::ErrCode;

constexpr pcnt_unit_t    UNIT    = PCNT_UNIT_0;
constexpr pcnt_channel_t CHANNEL = PCNT_CHANNEL_0;

bool         g_ready = false;
core::Millis g_windowStart{0};

/// APB saati 80 MHz → filtre değeri 12.5 ns adımlarla.
uint16_t filterTicksFromNs(uint16_t ns)
{
    const uint32_t ticks = (static_cast<uint32_t>(ns) * 80u) / 1000u;
    if (ticks == 0u)
    {
        return 1u;
    }
    // PCNT filtre alanı 10 bittir.
    return (ticks > 1023u) ? 1023u : static_cast<uint16_t>(ticks);
}

} // namespace

core::ErrCode begin(uint8_t pin, uint16_t filterNs)
{
    // ══ DAHİLİ PULL-UP — TASK-074 ════════════════════════════════════════════
    //
    // BU SATIR EKSİKTİ VE AKIŞ SENSÖRÜNÜN ÇALIŞMAMASININ ASIL NEDENİYDİ.
    //
    // YF-S401'in Hall çıkışı **açık kolektördür**: hattı yalnızca GND'ye
    // çeker, yukarı sürmez. Pull-up olmadan hat boşta kalır; PCNT ya hiç
    // darbe görmez ya da gürültüyü sayar.
    //
    // `pcnt_unit_config()` GPIO'yu PCNT girişine bağlar ama **pull-up'ı
    // etkinleştirmez** — sürücünün işi değildir. Sahada çalışan eski kodda
    // bu satır vardı (`pinMode(PIN_WATER_FLOW, INPUT_PULLUP)`) ve buraya
    // taşınırken düştü.
    //
    // SIRA ÖNEMLİ: pull-up PCNT yapılandırmasından ÖNCE kurulur ki hat
    // sayıcı devreye girdiği anda tanımlı bir seviyede olsun.
    //
    // NOT: GPIO 34-39'da dahili pull-up YOKTUR (ISSUE-002). `BoardPins.h`
    // `isSafePullupInput(FLOW_PULSE)` ile bunu derleme zamanında zorluyor.
    pinMode(static_cast<uint8_t>(pin), INPUT_PULLUP);

    pcnt_config_t cfg = {};
    cfg.pulse_gpio_num = static_cast<int>(pin);
    cfg.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    cfg.channel        = CHANNEL;
    cfg.unit           = UNIT;

    // DÜŞEN kenarda sayılır — sahada çalışan kodla aynı (`FALLING`).
    // Açık kolektör çıkışında anlamlı kenar, hattın GND'ye çekildiği andır.
    // Temiz bir kare dalgada iki kenar da aynı sayıyı verir; farkı ortadan
    // kaldırmak, sahadan gelen katsayının birebir geçerli kalmasını sağlar.
    cfg.pos_mode       = PCNT_COUNT_DIS;
    cfg.neg_mode       = PCNT_COUNT_INC;
    cfg.lctrl_mode     = PCNT_MODE_KEEP;
    cfg.hctrl_mode     = PCNT_MODE_KEEP;
    cfg.counter_h_lim  = COUNTER_LIMIT;
    cfg.counter_l_lim  = 0;

    if (pcnt_unit_config(&cfg) != ESP_OK)
    {
        core::diag::log(core::LogLevel::ERROR, ErrCode::SENSOR_FLOW_NO_PULSE,
                        static_cast<int32_t>(pin), "PCNT yapilandirilamadi");
        return ErrCode::CFG_VALIDATION_FAILED;
    }

    // Donanımsal gürültü filtresi: bu süreden kısa darbeler elenir.
    pcnt_set_filter_value(UNIT, filterTicksFromNs(filterNs));
    pcnt_filter_enable(UNIT);

    pcnt_counter_pause(UNIT);
    pcnt_counter_clear(UNIT);
    pcnt_counter_resume(UNIT);

    g_windowStart = core::Millis{millis()};
    g_ready       = true;
    return ErrCode::OK;
}

core::Result<PulseWindow> readAndReset()
{
    if (!g_ready)
    {
        return core::err<PulseWindow>(ErrCode::SYS_BOOT_STAGE_FAILED);
    }

    int16_t count = 0;

    // Okuma ile sıfırlama arasında darbe kaybını en aza indirmek için sayaç
    // önce duraklatılır. Duraklatma süresi mikrosaniyeler mertebesindedir.
    pcnt_counter_pause(UNIT);
    const esp_err_t rc = pcnt_get_counter_value(UNIT, &count);
    pcnt_counter_clear(UNIT);
    pcnt_counter_resume(UNIT);

    const core::Millis now = core::Millis{millis()};

    PulseWindow w;
    // GEÇEN SÜRE sayımla BİRLİKTE döndürülür — süre olmadan darbe sayısına
    // "debi" denemez (mevcut sistemin hatası buydu).
    w.elapsed     = core::elapsed(now, g_windowStart);
    w.pulses      = (count > 0) ? static_cast<uint32_t>(count) : 0u;
    w.overflow    = (count >= COUNTER_LIMIT);
    g_windowStart = now;

    if (rc != ESP_OK)
    {
        return core::err<PulseWindow>(ErrCode::SENSOR_FLOW_NO_PULSE);
    }
    if (w.overflow)
    {
        core::diag::log(core::LogLevel::WARNING, ErrCode::SENSOR_OUT_OF_RANGE,
                        static_cast<int32_t>(count), "PCNT sayaci tasti");
    }
    return core::ok(w);
}

uint32_t peek()
{
    if (!g_ready)
    {
        return 0;
    }
    int16_t count = 0;
    pcnt_get_counter_value(UNIT, &count);
    return (count > 0) ? static_cast<uint32_t>(count) : 0u;
}

} // namespace pulse
} // namespace hal
