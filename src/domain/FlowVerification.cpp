#include "domain/FlowVerification.h"

#include "core/Diagnostics.h"
#include "domain/ActuatorManager.h"

namespace domain {
namespace flow {
namespace {

using core::ActuatorId;
using core::ErrCode;
using core::Millis;
using core::SensorId;
using core::SensorQuality;

const core::Config* g_cfg    = nullptr;
VerifyPhase         g_phase  = VerifyPhase::INACTIVE;
ErrCode             g_reason = ErrCode::OK;
bool                g_latched = false;
bool                g_ready   = false;

const core::SensorSample* findFlow(const core::SystemState& snap)
{
    const uint8_t n = (snap.sensors.count <= core::MAX_SENSORS) ? snap.sensors.count
                                                                : core::MAX_SENSORS;
    for (uint8_t i = 0; i < n; ++i)
    {
        if (snap.sensors.samples[i].id == SensorId::WATER_FLOW)
        {
            return &snap.sensors.samples[i];
        }
    }
    return nullptr;
}

void latch(ErrCode why)
{
    if (g_latched) { return; }   // ilk neden korunur; log seli olmaz
    g_latched = true;
    g_reason  = why;
    g_phase   = VerifyPhase::FAILED;
    core::diag::raise(why);
    core::diag::log(core::LogLevel::CRITICAL, why, 0, "akis dogrulama basarisiz - mandallandi");
}

} // namespace

core::ErrCode begin(const core::Config& cfg)
{
    g_cfg     = &cfg;
    g_phase   = VerifyPhase::INACTIVE;
    g_reason  = ErrCode::OK;
    g_latched = false;
    g_ready   = true;
    return ErrCode::OK;
}

void evaluate(const core::SystemState& snap, Millis now)
{
    if (!g_ready || g_cfg == nullptr) { return; }

    // Mandal aktifken yeniden değerlendirme yapılmaz: koşul düzelse bile
    // kendiliğinden temizlenmemelidir (TASK-031 Karar 3).
    if (g_latched) { return; }

    const ActuatorRuntime& pump = actuators::runtime(ActuatorId::WATER_PUMP);

    // Pompa kapalıyken akış olmaması NORMALDİR — doğrulama yapılmaz.
    if (pump.isOn == 0u)
    {
        g_phase = VerifyPhase::INACTIVE;
        return;
    }

    // Gecikme, pompanın GERÇEK açılma anından sayılır. Ayrı bir sayaç
    // tutulmaz: hızlı kapat/aç döngüsünde `lastOnAt` her açılışta
    // güncellendiği için gecikme kendiliğinden sıfırlanır (Karar 5).
    if (!core::hasElapsed(now, pump.lastOnAt, core::Duration{g_cfg->safety.flowVerifyDelayMs}))
    {
        g_phase = VerifyPhase::PRIMING;
        return;
    }

    const core::SensorSample* f = findFlow(snap);

    // Sensör yok veya okunamıyor → KORUMA ÇALIŞMIYOR → pompayı çalıştırma.
    if (f == nullptr || f->quality != SensorQuality::OK)
    {
        latch(ErrCode::SAFETY_FLOW_VERIFY_FAILED);
        return;
    }

    // Kalite OK ve debi eşiğin altında → GERÇEK kuru çalışma.
    if (f->value < g_cfg->safety.flowMinRate)
    {
        latch(ErrCode::SAFETY_DRY_RUN);
        return;
    }

    g_phase = VerifyPhase::VERIFIED;
}

bool        latched() { return g_latched; }
ErrCode     reason()  { return g_reason; }
VerifyPhase phase()   { return g_phase; }

void acknowledge()
{
    if (!g_latched) { return; }
    core::diag::clear(g_reason);
    core::diag::log(core::LogLevel::WARNING, g_reason, 0, "akis mandali operator onayiyla temizlendi");
    g_latched = false;
    g_reason  = ErrCode::OK;
    g_phase   = VerifyPhase::INACTIVE;
}

} // namespace flow
} // namespace domain
