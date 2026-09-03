#include "domain/AutomationEngine.h"

#include "core/Diagnostics.h"
#include "core/Rule.h"
#include "core/StateStore.h"
#include "domain/ActuatorManager.h"
#include "domain/RuleEvaluator.h"
#include "services/ConfigService.h"

namespace domain {
namespace automation {
namespace {

using core::ActuatorId;
using core::AutomationMode;
using core::ErrCode;
using core::Millis;
using core::RuleVerdict;

const core::Config* g_cfg = nullptr;
bool                g_ready = false;

core::RuleRuntime g_rt[core::MAX_RULES];

/// Motorun en son gördüğü kural kümesi sürümü. Operatör kuralları
/// düzenlediğinde slotların ANLAMI değişir; eski histerezis ve son tetikleme
/// zamanını devralmak, yeni kuralın yanlış taraftan başlaması demektir.
uint32_t g_seenRulesRevision = 0;

/// Aktüatör başına manuel override. Global DEĞİL: hava pompasına müdahale,
/// su pompasının otomasyonunu durdurmamalı.
struct Override
{
    Millis  since;
    uint8_t active;
};
Override g_override[core::MAX_ACTUATORS];

uint8_t g_activeRuleId = 0xFFu;

/// Snapshot'ta bir sensörü arar.
const core::SensorSample* findSensor(const core::SystemState& s, core::SensorId id)
{
    const uint8_t n = (s.sensors.count <= core::MAX_SENSORS) ? s.sensors.count
                                                             : core::MAX_SENSORS;
    for (uint8_t i = 0; i < n; ++i)
    {
        if (s.sensors.samples[i].id == id) { return &s.sensors.samples[i]; }
    }
    return nullptr;
}

bool overrideActive(ActuatorId id, Millis now)
{
    const uint8_t i = static_cast<uint8_t>(id);
    if (i >= core::MAX_ACTUATORS || g_override[i].active == 0u) { return false; }

    if (core::hasElapsed(now, g_override[i].since,
                         core::millisecs(g_cfg->automation.manualOverrideMs)))
    {
        // Süre doldu: otomasyon kontrolü GERİ ALIYOR. Sessizce dönmek kafa
        // karıştırır — kaydediyoruz.
        g_override[i].active = 0u;

        // Kaynağı GERİ VER. Bu olmadan tahkim kilitlenir: `rt.source`
        // MANUAL kaldığı için otomasyonun her isteği `REJECTED_MODE`
        // döner ve otomasyon o aktüatörü bir daha hiç kontrol edemez.
        actuators::releaseSource(id);

        core::diag::log(core::LogLevel::INFO, ErrCode::OK, static_cast<int32_t>(i),
                        "manuel override suresi doldu - otomasyon devraldi");
        return false;
    }
    return true;
}

uint32_t overrideRemainingMs(ActuatorId id, Millis now)
{
    const uint8_t i = static_cast<uint8_t>(id);
    if (i >= core::MAX_ACTUATORS || g_override[i].active == 0u) { return 0u; }

    const uint32_t total   = g_cfg->automation.manualOverrideMs;
    const uint32_t elapsed = core::elapsed(now, g_override[i].since).ms;
    return (elapsed >= total) ? 0u : (total - elapsed);
}

} // namespace

core::ErrCode begin(const core::Config& cfg)
{
    g_cfg = &cfg;
    for (uint8_t i = 0; i < core::MAX_RULES; ++i) { g_rt[i].reset(); }
    for (uint8_t i = 0; i < core::MAX_ACTUATORS; ++i) { g_override[i] = Override{Millis{0}, 0u}; }
    g_activeRuleId       = 0xFFu;
    g_seenRulesRevision  = services::config::rulesRevision();
    g_ready              = true;
    return ErrCode::OK;
}

void evaluate(const core::SystemState& snap, bool timeValid, Millis now)
{
    if (!g_ready || g_cfg == nullptr) { return; }

    // ── Kural kümesi değiştiyse çalışma durumları sıfırlanır ───────────────
    // Kurallar web arayüzünden `net` task'ında yazılır, burada `app_core`'da
    // okunur. Sürüm sayacı tek yazarlıdır; onu izlemek, düzenlenen bir slotun
    // eski histerezisiyle çalışmasını önler. Mod değişiminde uygulanan kural
    // (setMode) ile aynı gerekçe.
    const uint32_t rev = services::config::rulesRevision();
    if (rev != g_seenRulesRevision)
    {
        for (uint8_t i = 0; i < core::MAX_RULES; ++i) { g_rt[i].reset(); }
        g_activeRuleId      = 0xFFu;
        g_seenRulesRevision = rev;
        core::diag::log(core::LogLevel::INFO, ErrCode::OK, static_cast<int32_t>(rev),
                        "kural kumesi degisti — durumlar sifirlandi");
    }

    // Override süreleri MOD'DAN BAĞIMSIZ işler. Yalnızca kural hedefi olan
    // aktüatörleri kontrol etseydik, kuralı olmayan bir aktüatörün override'ı
    // hiç sona ermez ve `releaseSource()` hiç çağrılmazdı.
    for (uint8_t i = 0; i < core::MAX_ACTUATORS; ++i)
    {
        (void)overrideActive(static_cast<ActuatorId>(i), now);
    }

    // ── MANUAL modda kurallar HİÇ değerlendirilmez ─────────────────────────
    // Yalnızca "istek üretilmez" değil: zamanlayıcılar da ilerlemez, böylece
    // AUTO'ya geçildiğinde kurallar TEMİZ bir durumdan başlar — yarı
    // ilerlemiş bir histerezisle değil.
    //
    // Bu aynı zamanda M4 kapısının kapalı kalma mekanizmasıdır.
    if (g_cfg->automation.mode != AutomationMode::AUTO) { return; }

    // Aktüatör başına kazanan istek.
    RuleVerdict best[core::MAX_ACTUATORS];
    for (uint8_t i = 0; i < core::MAX_ACTUATORS; ++i) { best[i] = core::noVerdict(); }

    uint8_t winnerRule[core::MAX_ACTUATORS];
    for (uint8_t i = 0; i < core::MAX_ACTUATORS; ++i) { winnerRule[i] = 0xFFu; }

    const uint8_t n = (g_cfg->rules.count <= core::MAX_RULES) ? g_cfg->rules.count
                                                              : core::MAX_RULES;
    for (uint8_t i = 0; i < n; ++i)
    {
        const core::Rule& r = g_cfg->rules.rules[i];
        if (r.enabled == 0u || r.kind == core::RuleKind::INACTIVE) { continue; }

        const uint8_t ti = static_cast<uint8_t>(r.target);
        if (ti >= core::MAX_ACTUATORS) { continue; }

        // Operatör müdahale ettiyse otomasyon O AKTÜATÖR için susar.
        if (overrideActive(r.target, now)) { continue; }

        RuleVerdict v;
        if (r.kind == core::RuleKind::THRESHOLD)
        {
            v = rules::evaluateThreshold(r, g_rt[i], findSensor(snap, r.sensor), now);
        }
        else
        {
            v = rules::evaluateSchedule(r, g_rt[i], timeValid, snap.time.epoch, now);
        }

        if (v.applies == 0u) { continue; }

        // Çakışma: yüksek öncelik kazanır. Eşit öncelik doğrulamada zaten
        // reddediliyor (TASK-054); yine de belirsizlik bırakmıyoruz —
        // DİZİDEKİ İLK kural kazanır ve bu davranış belgelidir.
        if (best[ti].applies == 0u || v.priority > best[ti].priority)
        {
            best[ti]       = v;
            winnerRule[ti] = i;
        }
    }

    // ── İstekleri ActuatorManager'a ilet ───────────────────────────────────
    g_activeRuleId = 0xFFu;
    for (uint8_t i = 0; i < core::MAX_ACTUATORS; ++i)
    {
        // Hiçbir kural bu aktüatör hakkında konuşmuyorsa DOKUNULMAZ.
        // Aksi hâlde kural tanımlamak, tanımlanmamış aktüatörleri kapatmak
        // anlamına gelirdi.
        if (best[i].applies == 0u) { continue; }

        const core::CommandResult r = actuators::request(
            static_cast<ActuatorId>(i), best[i].wantOn != 0u,
            core::ControlSource::AUTOMATION, now);

        // Sonuç KAYDEDİLİR ama davranış DEĞİŞMEZ: kural bir sonraki döngüde
        // aynı isteği yine üretir ve kısıt/kilit kalkınca kendiliğinden
        // uygulanır (§11.4).
        if (r != core::CommandResult::ACCEPTED && r != core::CommandResult::NO_CHANGE)
        {
            core::diag::log(core::LogLevel::INFO, ErrCode::SAFETY_BLOCKED,
                            static_cast<int32_t>(r), "otomasyon istegi uygulanmadi");
        }

        if (best[i].wantOn != 0u) { g_activeRuleId = winnerRule[i]; }
    }
}

void noteManualCommand(ActuatorId id, Millis now)
{
    const uint8_t i = static_cast<uint8_t>(id);
    if (i >= core::MAX_ACTUATORS) { return; }

    g_override[i].since  = now;
    g_override[i].active = 1u;

    core::diag::log(core::LogLevel::INFO, ErrCode::OK, static_cast<int32_t>(i),
                    "manuel override basladi");
}

core::ErrCode setMode(AutomationMode m)
{
    if (g_cfg == nullptr) { return ErrCode::CFG_VALIDATION_FAILED; }

    core::AutomationConfig a = g_cfg->automation;
    a.mode                   = m;

    const core::ConfigError e = services::config::updateAutomation(a);
    if (e.code != ErrCode::OK) { return e.code; }

    // Mod değişiminde kural durumları sıfırlanır: AUTO'ya geçerken yarı
    // ilerlemiş bir histerezis devralınmamalı.
    for (uint8_t i = 0; i < core::MAX_RULES; ++i) { g_rt[i].reset(); }

    core::diag::log(core::LogLevel::WARNING, ErrCode::OK, static_cast<int32_t>(m),
                    (m == AutomationMode::AUTO) ? "otomasyon ETKIN" : "otomasyon KAPALI");
    return ErrCode::OK;
}

AutomationMode mode()
{
    return (g_cfg != nullptr) ? g_cfg->automation.mode : AutomationMode::MANUAL;
}

core::ErrCode publish(const core::SystemState& snap, bool timeValid, Millis now)
{
    if (g_cfg == nullptr) { return ErrCode::CFG_VALIDATION_FAILED; }

    core::AutomationStatus s{};
    s.mode         = g_cfg->automation.mode;
    s.activeRuleId = g_activeRuleId;

    // Saat geçersizse ÇİZELGELER duraklamıştır — arayüz bunu göstermeli.
    // Eşik kuralları çalışmaya devam eder (§11.2).
    s.schedulesPaused = timeValid ? 0u : 1u;

    // Kalan override süresi: en uzun olanı yayınlanır (arayüzde tek sayı).
    uint32_t longest = 0u;
    for (uint8_t i = 0; i < core::MAX_ACTUATORS; ++i)
    {
        const uint32_t r = overrideRemainingMs(static_cast<ActuatorId>(i), now);
        if (r > longest) { longest = r; }
    }
    s.overrideRemaining = core::millisecs(longest);

    s.nextScheduleAt = rules::nextScheduleAt(g_cfg->rules, timeValid, snap.time.epoch);

    return core::state::publishAutomation(s);
}

} // namespace automation
} // namespace domain
