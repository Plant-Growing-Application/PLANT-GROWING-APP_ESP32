#include "AnalogSensors.h"

#include <math.h>

#include "core/BoardPins.h"
#include "hal/AdcInput.h"

namespace services {
namespace sensors {
namespace {

/// Değer sayısal olarak kullanılabilir mi?
inline bool usable(float v)
{
    return !isnan(v) && !isinf(v);
}

} // namespace

// ===========================================================================
// Su sıcaklığı — NTC
// ===========================================================================

core::ErrCode WaterTempSensor::begin(const core::SensorConfig& cfg)
{
    (void)cfg;
    const core::ErrCode rc = hal::adc::configurePin(board::ADC_WATER_TEMP);
    _ready                 = (rc == core::ErrCode::OK);
    return rc;
}

RawSample WaterTempSensor::sample(const SampleContext& ctx)
{
    (void)ctx;
    if (!_ready)
    {
        return rawFault();
    }

    const core::Result<hal::AdcSample> r = hal::adc::read(board::ADC_WATER_TEMP);
    if (r.failed())
    {
        return rawFault();
    }

    // ══ ORANSAL (RATIOMETRIC) HESAP — TASK-074 ═══════════════════════════════
    //
    // Bu fonksiyon eskiden `millivolts` üzerinden çalışıyordu ve SAHADA YANLIŞ
    // DEĞER ÜRETİYORDU:
    //
    //   R_ntc = R_series × (VCC_MV − mv) / mv     ← VCC_MV = 3300 varsayımı
    //
    // Hata şurada: `mv`, `esp_adc_cal_raw_to_voltage()` ile 12 dB
    // zayıflatmada üretiliyor ve ESP32 ADC'si o modda **3,1 V civarında
    // doyuma girer** — 3300 mV'a HİÇ ulaşmaz. Yani `(VCC_MV − mv)` payı
    // sistematik olarak kayıyor, ADC'nin tam ölçeği besleme gerilimine eşit
    // olmadığı için oran bozuluyordu.
    //
    // Çözüm, çalıştığı bilinen sahadaki formülün yaptığı şey: **ham ADC
    // oranıyla** çalışmak. Bölücü oranı zaten VCC'den bağımsızdır ve mutlak
    // gerilim kalibrasyonuna hiç ihtiyaç duymaz:
    //
    //   V_out/VCC = raw/4095
    //   Bölücü (NTC → VCC, R_series → GND):
    //       V_out = VCC × R_series / (R_ntc + R_series)
    //   ⇒   4095/raw − 1 = R_ntc / R_series
    //
    // Sahadaki formülün `log((4095.0/sensorValue) - 1.0)` ifadesi tam olarak
    // budur. Buradaki fark yalnızca DOMAIN KORUMALARI ve `R_NOMINAL`'ın ayrı
    // bir sabit olarak kalması: ikisi eşitken sonuç birebir aynı, farklı bir
    // NTC takıldığında ise formül doğru kalır.
    const float raw = static_cast<float>(r.value.raw);

    // --- DOMAIN KORUMASI 1: bölücü uçlarında direnç hesaplanamaz ---
    //
    // raw = 0     → 4095/0        = ∞  → log(∞) tanımsız (NTC kopuk)
    // raw = 4095  → 4095/4095 − 1 = 0  → log(0) = −∞    (NTC kısa devre)
    //
    // Sahadaki formülün üç matematiksel domain hatasından ikisi bunlardı ve
    // sonuç sessizce kullanılıyordu (REQUIREMENTS §3.1). Değer üretmek yerine
    // ARIZA bildiriyoruz.
    if (!(raw >= 1.0f) || raw >= static_cast<float>(hal::adc::ADC_MAX_RAW))
    {
        return rawFault();
    }

    const float ratio = (static_cast<float>(hal::adc::ADC_MAX_RAW) / raw) - 1.0f;
    const float rNtc  = ntc::R_SERIES * ratio;

    // --- DOMAIN KORUMASI 2: log() pozitif argüman ister ---
    if (!(rNtc > 0.0f) || !usable(rNtc))
    {
        return rawFault();
    }

    // Beta denklemi:
    //   1/T = 1/T0 + (1/Beta) × ln(R / R0)
    const float invT =
        (1.0f / ntc::T_NOMINAL_K) + (1.0f / ntc::BETA) * logf(rNtc / ntc::R_NOMINAL);

    // --- DOMAIN KORUMASI 3: sıfıra bölme ---
    if (!usable(invT) || fabsf(invT) < 1e-9f)
    {
        return rawFault();
    }

    const float celsius = (1.0f / invT) - 273.15f;
    if (!usable(celsius))
    {
        return rawFault();
    }

    RawSample s = rawOk(celsius);
    // ADC uçta değil ama sınıra yakınsa şüphe işaretlenir; FAULT kararı
    // hattın aralık kontrolüne bırakılır (yanlış pozitif FAULT, çalışan bir
    // sistemi durdurur — TASK-023 Karar 5).
    s.suspect = r.value.atRail;
    return s;
}

// ===========================================================================
// pH
// ===========================================================================

core::ErrCode PhSensor::begin(const core::SensorConfig& cfg)
{
    (void)cfg;
    const core::ErrCode rc = hal::adc::configurePin(board::ADC_PH);
    _ready                 = (rc == core::ErrCode::OK);
    return rc;
}

RawSample PhSensor::sample(const SampleContext& ctx)
{
    (void)ctx;
    if (!_ready)
    {
        return rawFault();
    }

    const core::Result<hal::AdcSample> r = hal::adc::read(board::ADC_PH);
    if (r.failed())
    {
        return rawFault();
    }

    const float mv = static_cast<float>(r.value.millivolts);

    // MV_PER_PH derleme zamanı sabiti ve sıfır olamaz; yine de bölme
    // korunur — sabit ileride yapılandırılabilir hale gelirse hata
    // sessizce geri gelmesin.
    if (fabsf(ph::MV_PER_PH) < 1e-6f)
    {
        return rawFault();
    }

    // 2 nokta kalibrasyonun eğim/ofset kısmı hatta (`scale`/`offset`)
    // uygulanır; burada nominal dönüşüm yapılır.
    const float phValue = 7.0f + (mv - ph::MV_AT_PH7) / ph::MV_PER_PH;

    if (!usable(phValue))
    {
        return rawFault();
    }

    RawSample s = rawOk(phValue);
    s.suspect   = r.value.atRail;
    return s;
}

// ===========================================================================
// EC — sıcaklık telafili
// ===========================================================================

core::ErrCode EcSensor::begin(const core::SensorConfig& cfg)
{
    (void)cfg;
    const core::ErrCode rc = hal::adc::configurePin(board::ADC_EC);
    _ready                 = (rc == core::ErrCode::OK);
    return rc;
}

RawSample EcSensor::sample(const SampleContext& ctx)
{
    if (!_ready)
    {
        return rawFault();
    }

    const core::Result<hal::AdcSample> r = hal::adc::read(board::ADC_EC);
    if (r.failed())
    {
        return rawFault();
    }

    const float mv       = static_cast<float>(r.value.millivolts);
    float       ecRaw    = mv * ec::MS_PER_MV;
    bool        lowConf  = false;

    if (!usable(ecRaw) || ecRaw < 0.0f)
    {
        return rawFault();
    }

    // --- SICAKLIK TELAFİSİ ---
    //
    // Bağımlılık `SampleContext` üzerinden AÇIK ve TEK YÖNLÜDÜR: EC sensörü
    // sıcaklık sensörünü çağırmaz, verilen bağlamı kullanır (TASK-022 Karar 5).
    if (ctx.waterTempValid)
    {
        const float denom = 1.0f + ec::TEMP_COEFF * (ctx.waterTempC - ec::REF_TEMP_C);

        // DOMAIN KORUMASI: T ≈ −25 °C'de payda sıfıra yaklaşır.
        if (fabsf(denom) < 0.1f)
        {
            lowConf = true;  // telafi güvenilir değil, ham değerle devam
        }
        else
        {
            const float compensated = ecRaw / denom;
            if (usable(compensated) && compensated >= 0.0f)
            {
                ecRaw = compensated;
            }
            else
            {
                lowConf = true;
            }
        }
    }
    else
    {
        // Sıcaklık geçersiz → telafi YAPILAMAZ.
        //
        // Değer yine yayınlanır (kullanıcı görsün) ama güveni düşürülür:
        // hat bunu STALE'e çevirir ve otomasyon bu değere GÜVENMEZ.
        // Sessizce telafisiz değer yayınlamak, yanlış bir ölçümü doğru
        // gibi göstermek olurdu.
        lowConf = true;
    }

    RawSample s      = rawOk(ecRaw);
    s.suspect        = r.value.atRail;
    s.lowConfidence  = lowConf;
    return s;
}

} // namespace sensors
} // namespace services
