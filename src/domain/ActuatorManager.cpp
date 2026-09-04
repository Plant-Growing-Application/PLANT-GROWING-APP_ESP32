#include "domain/ActuatorManager.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/Diagnostics.h"
#include "core/StateStore.h"
#include "hal/RelayOutput.h"
#include "services/ConfigService.h"

namespace domain {
namespace actuators {
namespace {

using core::ActuatorId;
using core::CommandResult;
using core::ControlSource;
using core::ErrCode;
using core::Millis;

constexpr uint8_t COUNT = core::MAX_ACTUATORS;

ActuatorRuntime  g_rt[COUNT];
const core::Config* g_cfg    = nullptr;

/// Aktüatör kısıtlarının TUTARLI kopyası (TASK-072).
///
/// Canlı `g_config.actuators[i]` üç süre alanını birlikte taşır ve `net`
/// task'ı tamamını tek seferde değiştirir (web'den kısıt kaydetme veya
/// "Bağlı cihazlar" onay kutusu). Buradan alan alan okumak, hiç var olmamış
/// bir `minRunMs`/`maxRunMs` kombinasyonuyla çalışmak demekti.
///
/// Kopya her `apply()` turunun başında tazelenir. Sonuç: bir config
/// değişikliği en geç bir sonraki 100 ms'lik turda etkili olur — başlıkta
/// zaten vaat edilen davranış budur.
core::ActuatorConfig g_actCfg[core::MAX_ACTUATORS] = {};
SafetyPermitFn      g_permit = nullptr;
TaskHandle_t        g_owner  = nullptr;
bool                g_ready  = false;

inline uint8_t idx(ActuatorId id) { return static_cast<uint8_t>(id); }

inline bool valid(ActuatorId id) { return idx(id) < COUNT; }

/// Yapılandırmada etkin ve fiziksel bir rölesi var mı?
inline bool usable(ActuatorId id)
{
    return valid(id) && g_cfg != nullptr && g_actCfg[idx(id)].enabled != 0u &&
           hal::relay::isMapped(id);
}

/// Çalışma süresini biriktirir. Yalnızca KAPANIRKEN çağrılır; çalışırken
/// biriken süre `publish()` içinde anlık olarak eklenir (çift sayım olmaz).
void accumulateRun(ActuatorRuntime& rt, Millis now)
{
    if (rt.isOn != 0u)
    {
        rt.totalRunMs += core::elapsed(now, rt.lastOnAt).ms;
    }
}

/// Röleyi sürer ve sayaçları günceller. Röle GPIO'suna giden TEK yol budur.
void drive(ActuatorId id, ActuatorRuntime& rt, bool on, Millis now)
{
    const ErrCode e = hal::relay::set(id, on);
    if (e != ErrCode::OK)
    {
        core::diag::raise(e, static_cast<int32_t>(idx(id)));
        return;
    }

    if (on)
    {
        rt.lastOnAt = now;
        rt.isOn     = 1u;
        rt.everRan  = 1u;
        if (rt.cycleCount < 0xFFFFu) { rt.cycleCount++; }
    }
    else
    {
        accumulateRun(rt, now);
        rt.lastOffAt = now;
        rt.isOn      = 0u;
    }
}

/// `apply()`in yalnızca `app_core`'dan çağrıldığını doğrular.
///
/// `StateStore`'daki tek-yazar doğrulamasıyla aynı desen: REDDETMEZ, raporlar.
/// Bir denetimin güvenlik yolunu kilitlemesi, ihlalin kendisinden kötüdür.
void verifyOwner()
{
    TaskHandle_t self = xTaskGetCurrentTaskHandle();
    if (g_owner == nullptr)
    {
        g_owner = self;
    }
    else if (g_owner != self)
    {
        core::diag::log(core::LogLevel::CRITICAL, ErrCode::ACTUATOR_STATE_MISMATCH, 0,
                        "aktuator tek-task kurali ihlali");
    }
}

} // namespace

core::ErrCode begin(const core::Config& cfg, SafetyPermitFn permit)
{
    if (permit == nullptr) { return ErrCode::CFG_VALIDATION_FAILED; }

    g_cfg    = &cfg;
    g_permit = permit;
    g_owner  = nullptr;

    // İlk kopya BURADA alınır: `request()` ilk `apply()`den önce çağrılabilir
    // (komut kuyruğu döngünün başında işlenir) ve sıfırlanmış bir kısıt
    // tablosuyla karar vermek `minRunMs = 0`, `maxRunMs = 0` demek olurdu —
    // yani hiç koruma yok.
    services::config::copyActuators(g_actCfg);

    // Boot'ta durum GERİ YÜKLENMEZ (ARCHITECTURE §19) — her boot rölesiz başlar.
    for (uint8_t i = 0; i < COUNT; ++i) { g_rt[i].reset(); }

    g_ready = true;
    return ErrCode::OK;
}

CommandResult request(ActuatorId id, bool on, ControlSource source, Millis now)
{
    if (!g_ready || !usable(id)) { return CommandResult::REJECTED_INVALID; }

    ActuatorRuntime&           rt  = g_rt[idx(id)];
    const core::ActuatorConfig& cfg = g_actCfg[idx(id)];
    const uint8_t              want = on ? 1u : 0u;

    if (rt.desired == want && rt.isOn == want) { return CommandResult::NO_CHANGE; }

    // Tahkim (§10.3): düşük öncelikli kaynak, yüksek öncelikli kaynağın
    // belirlediği durumu geçersiz kılamaz.
    // KİLİTLENME KORUMASI: bir kez MANUAL komut geldiğinde `rt.source`
    // MANUAL olur ve `sourceOutranks(AUTOMATION, MANUAL)` sonsuza kadar
    // false döner. `automation::` override süresi dolunca
    // `releaseSource()` çağırarak kaynağı geri verir (TASK-057 Karar 2).
    // O çağrı olmadan otomasyon o aktüatörü BİR DAHA HİÇ kontrol edemezdi.
    if (!actuator::sourceOutranks(source, rt.source))
    {
        core::diag::log(core::LogLevel::INFO, ErrCode::ACTUATOR_STATE_MISMATCH,
                        static_cast<int32_t>(idx(id)), "dusuk oncelikli kaynak reddedildi");
        return CommandResult::REJECTED_MODE;
    }

    if (on)
    {
        // Güvenlik izni TAZE sorulur — önbelleğe alınmış izin kullanılmaz.
        const ErrCode permitted = g_permit(id);
        if (permitted != ErrCode::OK)
        {
            // MANDALLANMAZ. Engel kalkınca kendiliğinden çalışmamalı (P6).
            rt.blockReason = permitted;
            core::diag::log(core::LogLevel::WARNING, permitted, static_cast<int32_t>(idx(id)),
                            "acma talebi guvenlik tarafindan reddedildi");
            return CommandResult::REJECTED_SAFETY;
        }

        rt.desired = 1u;
        rt.source  = source;

        const ConstraintVerdict v = actuator::canTurnOn(cfg, rt, now);
        if (!v.allowed)
        {
            rt.blockReason = v.reason;
            core::diag::log(core::LogLevel::INFO, v.reason, static_cast<int32_t>(idx(id)),
                            "acma ertelendi");
            return v.result;   // niyet mandallandı, apply() tekrar deneyecek
        }
    }
    else
    {
        rt.desired = 0u;
        rt.source  = source;

        const ConstraintVerdict v = actuator::canTurnOff(cfg, rt, now);
        if (!v.allowed)
        {
            rt.blockReason = v.reason;
            core::diag::log(core::LogLevel::INFO, v.reason, static_cast<int32_t>(idx(id)),
                            "kapatma ertelendi");
            return v.result;   // niyet mandallandı, apply() tekrar deneyecek
        }
    }

    rt.blockReason = ErrCode::OK;
    return CommandResult::ACCEPTED;
}

void apply(Millis now)
{
    if (!g_ready) { return; }
    verifyOwner();

    // Kısıt tablosunun tutarlı kopyası — tur başına BİR kez (TASK-072).
    // `request()` bu turun kopyasını değil bir öncekini kullanır; config
    // değişikliği en geç bir sonraki turda etkili olur.
    services::config::copyActuators(g_actCfg);

    for (uint8_t i = 0; i < COUNT; ++i)
    {
        const ActuatorId id = static_cast<ActuatorId>(i);
        ActuatorRuntime& rt = g_rt[i];

        if (!usable(id))
        {
            rt.desired = 0u;
            continue;
        }

        const core::ActuatorConfig& cfg = g_actCfg[i];

        // 1) GERÇEK pin durumu. Talep ile gerçek arasındaki fark hata göstergesidir.
        const bool actual = hal::relay::isOn(id);
        if ((actual ? 1u : 0u) != rt.isOn)
        {
            core::diag::raise(ErrCode::ACTUATOR_STATE_MISMATCH, static_cast<int32_t>(i));
            rt.isOn = actual ? 1u : 0u;
            if (actual) { rt.lastOnAt = now; }   // bilinmeyen süre; şimdiden say
        }

        // 2) maxRunMs aşımı → ZORLA kapat. Kısıt tanımaz, tahkim tanımaz.
        //    Tek seferlik aşım uzun bir sulama olabilir; TEKRARLAYAN aşım
        //    sistemik arızadır ve SafetyMonitor bunu acil duruma çevirir.
        if (actuator::maxRunExceeded(cfg, rt, now, g_cfg->safety.maxRuntimeGraceMs))
        {
            drive(id, rt, false, now);
            rt.desired     = 0u;
            rt.source      = ControlSource::SAFETY;
            rt.blockReason = ErrCode::ACTUATOR_MAX_RUNTIME;
            if (rt.maxRunViolations < 0xFFFFu) { rt.maxRunViolations++; }
            core::diag::log(core::LogLevel::WARNING, ErrCode::ACTUATOR_MAX_RUNTIME,
                            static_cast<int32_t>(rt.maxRunViolations), "maks sure asildi");
            continue;
        }

        // 3) CALISMA SIRASINDA IZLEME (§12.1 Katman 2).
        //    Enerjili bir aktuator icin izin kalktiysa DERHAL kapatilir —
        //    `minRunMs` uygulanmaz. Su seviyesi dusunce calisan pompanin
        //    kisa cevrim korumasi yuzunden 5 saniye daha kuru calismasi
        //    kabul edilemez.
        if (rt.isOn != 0u)
        {
            const ErrCode permitted = g_permit(id);
            if (permitted != ErrCode::OK)
            {
                drive(id, rt, false, now);
                rt.desired     = 0u;
                rt.source      = ControlSource::SAFETY;
                rt.blockReason = permitted;
                core::diag::log(core::LogLevel::CRITICAL, permitted, static_cast<int32_t>(i),
                                "calisan aktuator guvenlik nedeniyle durduruldu");
                continue;
            }
        }

        // 4) Niyet ile gerçeği uzlaştır.
        if (rt.desired == 1u && rt.isOn == 0u)
        {
            const ErrCode permitted = g_permit(id);
            if (permitted != ErrCode::OK)
            {
                // Mandallanmış niyet güvenlik engeline takıldı → niyeti TEMİZLE.
                rt.desired     = 0u;
                rt.blockReason = permitted;
                core::diag::log(core::LogLevel::WARNING, permitted, static_cast<int32_t>(i),
                                "bekleyen acma niyeti guvenlik nedeniyle iptal");
                continue;
            }

            const ConstraintVerdict v = actuator::canTurnOn(cfg, rt, now);
            if (!v.allowed) { rt.blockReason = v.reason; continue; }

            drive(id, rt, true, now);
            rt.blockReason = ErrCode::OK;
        }
        else if (rt.desired == 0u && rt.isOn == 1u)
        {
            const ConstraintVerdict v = actuator::canTurnOff(cfg, rt, now);
            if (!v.allowed) { rt.blockReason = v.reason; continue; }

            drive(id, rt, false, now);
            rt.blockReason = ErrCode::OK;
        }
        else
        {
            rt.blockReason = ErrCode::OK;
        }
    }
}

void forceAllOff(ErrCode reason, Millis now)
{
    // Hazır olmasa bile röleler kapatılır: güvenlik yolu başlatma durumuna
    // bağlı olamaz.
    const ErrCode e = hal::relay::allSafe();

    for (uint8_t i = 0; i < COUNT; ++i)
    {
        ActuatorRuntime& rt = g_rt[i];
        accumulateRun(rt, now);
        if (rt.isOn != 0u) { rt.lastOffAt = now; }
        rt.isOn        = 0u;
        rt.desired     = 0u;
        rt.source      = ControlSource::SAFETY;
        rt.blockReason = reason;
    }

    core::diag::log(core::LogLevel::CRITICAL, reason, static_cast<int32_t>(e),
                    "tum aktuatorler zorla kapatildi");
}

void releaseSource(ActuatorId id)
{
    if (!valid(id)) { return; }

    // SAFETY kaynağı serbest bırakılmaz: güvenliğin belirlediği bir durumu
    // otomasyonun devralması, vetoyu delmek olurdu.
    if (g_rt[idx(id)].source == ControlSource::SAFETY) { return; }

    g_rt[idx(id)].source = ControlSource::AUTOMATION;
}

uint16_t maxRunViolations(ActuatorId id)
{
    return valid(id) ? g_rt[idx(id)].maxRunViolations : 0u;
}

void clearMaxRunViolations()
{
    for (uint8_t i = 0; i < COUNT; ++i) { g_rt[i].maxRunViolations = 0u; }
}

core::ErrCode publish(Millis now)
{
    core::ActuatorsStatus out{};
    out.count = COUNT;

    for (uint8_t i = 0; i < COUNT; ++i)
    {
        const ActuatorRuntime& rt = g_rt[i];
        core::ActuatorStatus&  s  = out.items[i];

        s.id          = static_cast<ActuatorId>(i);
        s.isOn        = rt.isOn;
        s.source      = rt.source;
        s.blockReason = rt.blockReason;
        s.cycleCount  = rt.cycleCount;
        s.lastChange  = (rt.isOn != 0u) ? rt.lastOnAt : rt.lastOffAt;

        // Çalışırken biriken süre burada ANLIK eklenir; `accumulateRun()`
        // yalnızca kapanışta biriktirdiği için çift sayım olmaz.
        s.totalRunMs = rt.totalRunMs + ((rt.isOn != 0u) ? core::elapsed(now, rt.lastOnAt).ms : 0u);
    }

    return core::state::publishActuators(out);
}

const ActuatorRuntime& runtime(ActuatorId id)
{
    static const ActuatorRuntime empty{};
    return valid(id) ? g_rt[idx(id)] : empty;
}

} // namespace actuators
} // namespace domain
