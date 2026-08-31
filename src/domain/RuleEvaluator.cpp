#include "domain/RuleEvaluator.h"

#include <time.h>

#include "core/Diagnostics.h"

namespace domain {
namespace rules {
namespace {

using core::ErrCode;
using core::Millis;
using core::Rule;
using core::RuleKind;
using core::RuleRuntime;
using core::RuleVerdict;
using core::SensorQuality;

/// Minimum tetikleme aralığı doldu mu?
///
/// Histerezis **değer gürültüsüne**, bu ise **hızlı salınıma** karşı korur.
/// İkisi farklı arıza modlarına bakar; biri diğerinin yerini tutmaz.
bool intervalElapsed(const Rule& r, const RuleRuntime& rt, Millis now)
{
    if (r.minTriggerIntervalS == 0u || rt.everRan == 0u) { return true; }
    return core::hasElapsed(now, rt.lastTriggerAt,
                            core::millisecs(static_cast<uint32_t>(r.minTriggerIntervalS) * 1000u));
}

RuleVerdict verdict(const Rule& r, bool on)
{
    return RuleVerdict{r.target, static_cast<uint8_t>(on ? 1 : 0), 1u, r.priority};
}

/// Durum değişimini kaydeder.
void markTrigger(RuleRuntime& rt, bool on, Millis now)
{
    rt.active        = on ? 1u : 0u;
    rt.lastTriggerAt = now;
    rt.everRan       = 1u;
}

} // namespace

core::RuleVerdict evaluateThreshold(const Rule& r, RuleRuntime& rt,
                                    const core::SensorSample* sample, Millis now)
{
    rt.lastEvalAt = now;

    if (r.kind != RuleKind::THRESHOLD || r.enabled == 0u) { return core::noVerdict(); }

    // ── YALNIZCA `OK` KALİTE İLE KARAR ─────────────────────────────────────
    // `FAULT`/`OUT_OF_RANGE` bir sensörle karar vermek, bozuk bir
    // termometreye bakıp ısıtıcıyı açmaya benzer. `STALE` de kabul edilmez:
    // bayat bir değer geçmişi anlatır, şimdiyi değil.
    const bool usable = (sample != nullptr) && (sample->quality == SensorQuality::OK);

    if (!usable)
    {
        // Anlık hata sistemi durdurmasın, kalıcı hata güvenli tarafa düşürsün.
        if (rt.suspect == 0u)
        {
            rt.suspect      = 1u;
            rt.suspectSince = now;
        }

        if (!core::hasElapsed(now, rt.suspectSince, core::millisecs(SUSPECT_GRACE_MS)))
        {
            // Süre içinde: MEVCUT DURUMU KORU.
            return rt.active ? verdict(r, true) : core::noVerdict();
        }

        // Süre aşıldı: kuralı KAPAT tarafına düşür ve bir kez uyar.
        if (rt.active != 0u)
        {
            markTrigger(rt, false, now);
            core::diag::log(core::LogLevel::WARNING, ErrCode::SENSOR_OUT_OF_RANGE,
                            static_cast<int32_t>(r.sensor),
                            "sensor kalitesi bozuk - otomasyon kurali kapatildi");
        }
        return verdict(r, false);
    }

    rt.suspect = 0u;

    // ── HİSTEREZİS ─────────────────────────────────────────────────────────
    // Yön iki eşikten TÜRETİLİR; ayrı bir bayrak yok, dolayısıyla çelişki de
    // yok (TASK-054 Karar 4).
    const float v       = sample->value;
    const bool  lowSide = (r.onThreshold < r.offThreshold);   // düşünce aç

    bool want = (rt.active != 0u);
    if (lowSide)
    {
        if (v <= r.onThreshold)       { want = true;  }
        else if (v >= r.offThreshold) { want = false; }
        // Arada: MEVCUT DURUMU KORU — histerezisin tamamı bu satırdadır.
    }
    else
    {
        if (v >= r.onThreshold)       { want = true;  }
        else if (v <= r.offThreshold) { want = false; }
    }

    if (want != (rt.active != 0u))
    {
        // Değişim isteniyor ama minimum aralık dolmadıysa ERTELE.
        // Aralık mevcut durumu bozmaz, yalnızca değişimi geciktirir.
        if (!intervalElapsed(r, rt, now)) { return verdict(r, rt.active != 0u); }
        markTrigger(rt, want, now);
    }

    return verdict(r, want);
}

core::RuleVerdict evaluateSchedule(const Rule& r, RuleRuntime& rt, bool timeValid,
                                   core::EpochSeconds epoch, Millis now)
{
    rt.lastEvalAt = now;

    if (r.enabled == 0u) { return core::noVerdict(); }
    if (r.kind != RuleKind::SCHEDULE_WINDOW && r.kind != RuleKind::SCHEDULE_CYCLE)
    {
        return core::noVerdict();
    }

    // ── ZAMAN GEÇERLİLİĞİ: MUTLAK (§11.2) ──────────────────────────────────
    // Geçersiz zamanla çizelge çalıştırmak öngörülemez sulama demektir.
    // Kural değerlendirilmez — ne açar ne kapatır; `applies == 0`.
    if (!timeValid || !core::isTimeValid(epoch))
    {
        if (rt.active != 0u) { markTrigger(rt, false, now); }
        return core::noVerdict();
    }

    const time_t t = static_cast<time_t>(epoch.s);
    tm           lt{};
    localtime_r(&t, &lt);

    const uint16_t nowMin = static_cast<uint16_t>(lt.tm_hour * 60 + lt.tm_min);
    const uint32_t sod    = static_cast<uint32_t>(lt.tm_hour) * 3600u +
                            static_cast<uint32_t>(lt.tm_min) * 60u +
                            static_cast<uint32_t>(lt.tm_sec);

    const bool want = (r.kind == RuleKind::SCHEDULE_WINDOW)
                          ? inWindow(nowMin, r.startMin, r.endMin)
                          : inCycle(sod, r.cycleOnS, r.cyclePeriodS);

    if (want != (rt.active != 0u))
    {
        if (!intervalElapsed(r, rt, now)) { return verdict(r, rt.active != 0u); }
        markTrigger(rt, want, now);
    }

    return verdict(r, want);
}

core::EpochSeconds nextScheduleAt(const core::RuleSet& rs, bool timeValid,
                                  core::EpochSeconds epoch)
{
    if (!timeValid || !core::isTimeValid(epoch)) { return core::EPOCH_INVALID; }

    const time_t t = static_cast<time_t>(epoch.s);
    tm           lt{};
    localtime_r(&t, &lt);
    const uint32_t sod = static_cast<uint32_t>(lt.tm_hour) * 3600u +
                         static_cast<uint32_t>(lt.tm_min) * 60u +
                         static_cast<uint32_t>(lt.tm_sec);

    uint32_t best = 0xFFFFFFFFu;   // şimdiden kaç saniye sonra

    for (uint8_t i = 0; i < rs.count && i < core::MAX_RULES; ++i)
    {
        const Rule& r = rs.rules[i];
        if (r.enabled == 0u) { continue; }

        uint32_t delta = 0xFFFFFFFFu;

        if (r.kind == RuleKind::SCHEDULE_CYCLE && r.cyclePeriodS > 0u)
        {
            const uint32_t pos = sod % r.cyclePeriodS;
            delta = (pos < r.cycleOnS) ? 0u : (r.cyclePeriodS - pos);
        }
        else if (r.kind == RuleKind::SCHEDULE_WINDOW)
        {
            const uint32_t startSec = static_cast<uint32_t>(r.startMin) * 60u;
            if (inWindow(static_cast<uint16_t>(sod / 60u), r.startMin, r.endMin))
            {
                delta = 0u;
            }
            else
            {
                delta = (startSec > sod) ? (startSec - sod) : (86400u - sod + startSec);
            }
        }

        if (delta < best) { best = delta; }
    }

    return (best == 0xFFFFFFFFu) ? core::EPOCH_INVALID
                                 : core::EpochSeconds{epoch.s + static_cast<int64_t>(best)};
}

} // namespace rules
} // namespace domain
