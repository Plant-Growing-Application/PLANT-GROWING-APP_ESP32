#include "SensorPipeline.h"

#include <math.h>

namespace services {
namespace sensors {
namespace pipeline {
namespace {

using core::SensorQuality;

/// Değer sayısal olarak geçerli mi? `NaN` ve sonsuz, bir formülün bölme veya
/// domain hatası verdiğini gösterir.
///
/// Mevcut sistemdeki NTC formülü `sensorValue == 0` veya tam ölçekte
/// `log()` domain hatası üretiyordu ve sonuç sessizce kullanılıyordu
/// (REQUIREMENTS §3.1).
inline bool isFinite(float v)
{
    return !isnan(v) && !isinf(v);
}

/// Üstel hareketli ortalama.
///
/// `strength = 0` → FİLTRESİZ. Su seviyesi için varsayılan budur: pompa
/// çalışırken seviye hızla düşer ve filtre gecikmesi kuru çalışma demektir.
inline float applyEma(float previous, float sample, uint8_t strength)
{
    if (strength == 0u)
    {
        return sample;
    }
    const float alpha = 1.0f / static_cast<float>(strength);
    return previous + alpha * (sample - previous);
}

} // namespace

core::SensorSample notPresent(core::SensorId id, core::Millis now)
{
    core::SensorSample out{};
    out.timestamp  = now;
    out.value      = 0.0f;
    out.id         = id;
    out.quality    = SensorQuality::NOT_PRESENT;
    out.faultCount = 0;
    return out;
}

core::SensorSample process(core::SensorId id, const RawSample& raw,
                           const core::SensorConfig& cfg, PipelineState& st,
                           core::Millis now)
{
    core::SensorSample out{};
    out.id        = id;
    out.timestamp = now;

    // -----------------------------------------------------------------------
    // 1) DONANIM HATASI — en ağır tanı, hemen döner.
    //
    // Kalite öncelik sırası: NOT_PRESENT > FAULT > OUT_OF_RANGE > STALE > OK.
    // En ağır tanı kazanır; hem kopuk hem aralık dışı bir sensörü
    // OUT_OF_RANGE göstermek "değer var ama yüksek" izlenimi verirdi.
    // -----------------------------------------------------------------------
    if (raw.hardwareFault || !isFinite(raw.value))
    {
        if (st.faultCount < 0xFFFFu)
        {
            ++st.faultCount;
        }
        out.value      = 0.0f;
        out.quality    = SensorQuality::FAULT;
        out.faultCount = st.faultCount;
        st.lastSampleAt = now;
        return out;
    }

    // -----------------------------------------------------------------------
    // 2) KALİBRASYON — genel trim (offset/scale).
    //
    // Sensöre özgü matematik (NTC Beta, pH eğrisi, darbe→L/dk) SENSÖRDE
    // yapıldı; burada yalnızca saha ayarı uygulanır.
    // -----------------------------------------------------------------------
    const float calibrated = raw.value * cfg.scale + cfg.offset;

    if (!isFinite(calibrated))
    {
        if (st.faultCount < 0xFFFFu)
        {
            ++st.faultCount;
        }
        out.value      = 0.0f;
        out.quality    = SensorQuality::FAULT;
        out.faultCount = st.faultCount;
        st.lastSampleAt = now;
        return out;
    }

    // -----------------------------------------------------------------------
    // 3) DEĞİŞİM HIZI SINIRI — yalnızca AÇIKSA.
    //
    // `maxChangePerSec == 0` → sınır KAPALI (varsayılan).
    //
    // DİKKAT: bu kontrol su seviyesine UYGULANMAMALIDIR. Pompa çalışırken
    // seviye gerçekten hızlı düşer; agresif bir sınır gerçek ve KRİTİK bir
    // düşüşü "imkânsız" sayıp atardı — koruma tam ihtiyaç anında körleşirdi.
    // Varsayılan config bu yüzden seviye için 0 bırakır.
    // -----------------------------------------------------------------------
    bool rejectedAsImplausible = false;

    if (st.initialized != 0u && cfg.maxChangePerSec > 0.0f)
    {
        const core::Duration dt = core::elapsed(now, st.lastSampleAt);
        if (dt.ms > 0u)
        {
            const float seconds  = static_cast<float>(dt.ms) / 1000.0f;
            const float maxDelta = cfg.maxChangePerSec * seconds;
            const float delta    = fabsf(calibrated - st.lastRawValue);
            if (delta > maxDelta)
            {
                // Örnek ATILIR: önceki değer korunur, sayaç artar.
                rejectedAsImplausible = true;
                if (st.faultCount < 0xFFFFu)
                {
                    ++st.faultCount;
                }
            }
        }
    }

    const float accepted = rejectedAsImplausible ? st.lastRawValue : calibrated;

    // -----------------------------------------------------------------------
    // 4) FİLTRE
    // -----------------------------------------------------------------------
    if (st.initialized == 0u)
    {
        // İlk örnek: filtreyi gerçek değerle başlat. Sıfırdan başlatmak,
        // ilk okumalarda gerçeğin çok altında değerler üretirdi.
        st.filtered    = accepted;
        st.initialized = 1;
        st.lastChangeAt = now;
        st.warmupCount = 0;
    }
    else
    {
        st.filtered = applyEma(st.filtered, accepted, cfg.filterStrength);
    }

    const float value = st.filtered;

    // -----------------------------------------------------------------------
    // 5) BAYATLAMA — değer uzun süre hiç değişmiyorsa şüphelidir.
    //
    // Eşik ADC gürültüsünün altında olmalı ki sabit bir ortamdaki gerçek
    // ölçüm yanlışlıkla STALE sayılmasın.
    // -----------------------------------------------------------------------
    if (fabsf(accepted - st.lastRawValue) > STALE_EPSILON)
    {
        st.lastChangeAt = now;
    }
    const bool stale = core::hasElapsed(now, st.lastChangeAt, STALE_TIMEOUT);

    // -----------------------------------------------------------------------
    // 6) FİLTRE ISINMASI
    //
    // Filtre gerçek değere yakınsayana kadar değer yayınlanır ama
    // OTOMASYONDA KULLANILMAZ. Sessizce yanlış değer vermektense
    // "henüz güvenilir değil" demek doğrudur.
    // -----------------------------------------------------------------------
    bool warmingUp = false;
    if (cfg.filterStrength > 0u)
    {
        if (st.warmupCount < cfg.filterStrength)
        {
            ++st.warmupCount;
            warmingUp = true;
        }
    }

    // -----------------------------------------------------------------------
    // 7) KALİTE KARARI — öncelik sırasıyla
    // -----------------------------------------------------------------------
    if (!cfg.validRange.contains(value))
    {
        if (st.faultCount < 0xFFFFu)
        {
            ++st.faultCount;
        }
        out.quality = SensorQuality::OUT_OF_RANGE;
    }
    else if (stale || warmingUp || rejectedAsImplausible || raw.lowConfidence)
    {
        // `lowConfidence`: sensör değeri üretebildi ama güvenilirliği düşük.
        // Örnek: EC ölçümü, su sıcaklığı geçersiz olduğu için telafi
        // edilemedi. Değer kullanıcıya gösterilir ama OTOMASYONDA
        // KULLANILMAZ (`isUsable(STALE) == false`).
        out.quality = SensorQuality::STALE;
    }
    else
    {
        out.quality = SensorQuality::OK;
    }

    st.lastRawValue = accepted;
    st.lastSampleAt = now;

    out.value      = value;
    out.faultCount = st.faultCount;
    return out;
}

} // namespace pipeline
} // namespace sensors
} // namespace services
