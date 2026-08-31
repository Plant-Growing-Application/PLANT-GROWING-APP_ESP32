#include "domain/SafetyMonitor.h"

#include "core/Diagnostics.h"
#include "core/StateStore.h"
#include "domain/ActuatorManager.h"
#include "domain/EmergencyStop.h"
#include "domain/FlowVerification.h"
#include "services/sensors/WaterLevelSensor.h"

namespace domain {
namespace safety {
namespace {

using core::ActuatorId;
using core::ErrCode;
using core::Millis;
using core::SensorId;
using core::SensorQuality;

const core::Config* g_cfg      = nullptr;
uint32_t            g_mask     = ILK_NONE;
uint32_t            g_prevMask = ILK_NONE;
uint32_t            g_external = ILK_NONE;   ///< TASK-031/032'nin ayarladığı bitler
Millis              g_latchedAt{0};
bool                g_ready    = false;

/// Snapshot'ta bir sensörü arar. Bulunamazsa `nullptr`.
const core::SensorSample* find(const core::SystemState& snap, SensorId id)
{
    const uint8_t n = (snap.sensors.count <= core::MAX_SENSORS) ? snap.sensors.count
                                                                : core::MAX_SENSORS;
    for (uint8_t i = 0; i < n; ++i)
    {
        if (snap.sensors.samples[i].id == id) { return &snap.sensors.samples[i]; }
    }
    return nullptr;
}

/// Su seviyesi kilitlerini hesaplar — FAIL-SAFE.
///
/// Üç durum ve üçünün de sonucu belirlidir:
///   sensör yok        → FAULT kilidi (hiç veri gelmemiş)
///   kalite != OK      → FAULT kilidi (okunamıyor)
///   kalite OK, düşük  → INSUFFICIENT kilidi
///
/// Okunamayan bir güvenlik sensörü **en kötü durum** kabul edilir. Bunun
/// tersi — "okuyamıyorum, o hâlde sorun yok" — kuru çalışmanın en yaygın
/// nedenidir.
uint32_t evaluateLevel(const core::SystemState& snap)
{
    if (g_cfg->safety.requireLevelSensor == 0u)
    {
        return ILK_NONE;   // operatörün bilinçli tercihi (varsayılan: 1)
    }

    const core::SensorSample* s = find(snap, SensorId::WATER_LEVEL);
    if (s == nullptr || s->quality != SensorQuality::OK)
    {
        return ILK_LEVEL_SENSOR_FAULT;
    }

    // Değer eşlemesi `WaterLevelSensor.h`'ta tanımlı:
    //   0.0 = CRITICAL, 1.0 = LOW_LEVEL, 2.0 = SUFFICIENT
    const float sufficient = static_cast<float>(
        static_cast<uint8_t>(services::sensors::WaterLevelState::SUFFICIENT));

    return (s->value < sufficient - 0.5f) ? ILK_LEVEL_INSUFFICIENT : ILK_NONE;
}

/// `maxRunMs` TEKRARLI aşımı sistemik arızadır (ARCHITECTURE §12.3).
///
/// Tek seferlik aşım uzun bir sulama olabilir. Eşiğe ulaşan tekrar ise
/// pompanın, rölenin veya kontrol mantığının bozuk olduğunu gösterir.
uint32_t evaluateMaxRuntime()
{
    const uint8_t limit = g_cfg->safety.maxRuntimeViolations;
    if (limit == 0u) { return ILK_NONE; }

    for (uint8_t i = 0; i < core::MAX_ACTUATORS; ++i)
    {
        if (actuators::maxRunViolations(static_cast<ActuatorId>(i)) >= limit)
        {
            return ILK_MAX_RUNTIME_REPEATED;
        }
    }
    return ILK_NONE;
}

/// Bir kilit maskesinin RAPORLANACAK nedeni.
///
/// `ILK_DRY_RUN` iki ayrı nedenle set edilebilir (gerçek kuru çalışma veya
/// akış sensörü arızası). `reasonOf()` sabit olduğu için gerçek nedeni
/// `flow` modülünden alırız — aksi hâlde kopuk bir kabloya "kuru çalışma"
/// denir ve operatör yanlış yerde arıza arar.
ErrCode reportedReason(uint32_t blocking)
{
    if ((blocking & ILK_DRY_RUN) != 0u && (blocking & ILK_EMERGENCY_LATCHED) == 0u &&
        (blocking & (ILK_LEVEL_SENSOR_FAULT | ILK_LEVEL_INSUFFICIENT)) == 0u)
    {
        return flow::reason();
    }
    return firstReason(blocking);
}

/// Yalnızca YENİ aktifleşen kilitleri loglar.
///
/// Her döngüde loglansaydı 64 kayıtlık halka tampon 6,4 saniyede dolar ve
/// asıl arıza kaydı silinirdi (TASK-030 Karar 5).
void logEdges(uint32_t now_, uint32_t prev)
{
    const uint32_t rising  = now_ & ~prev;
    const uint32_t falling = prev & ~now_;

    for (uint8_t b = 0; b < 5; ++b)
    {
        const uint32_t bit = 1u << b;

        // Kuru çalışma kodunu `flow` modülü zaten kendi PRECİSE koduyla
        // yükseltti (SAFETY_DRY_RUN veya SAFETY_FLOW_VERIFY_FAILED).
        // Burada tekrar yükseltmek yanlış kodu kayda geçirirdi.
        if (bit == ILK_DRY_RUN) { continue; }

        if (rising & bit)
        {
            core::diag::raise(reasonOf(static_cast<Interlock>(bit)), static_cast<int32_t>(bit));
        }
        else if (falling & bit)
        {
            core::diag::clear(reasonOf(static_cast<Interlock>(bit)));
            core::diag::log(core::LogLevel::INFO, reasonOf(static_cast<Interlock>(bit)),
                            static_cast<int32_t>(bit), "kilit kalkti");
        }
    }
}

} // namespace

core::ErrCode begin(const core::Config& cfg)
{
    g_cfg      = &cfg;
    g_mask     = ILK_NONE;
    g_prevMask = ILK_NONE;
    g_external = ILK_NONE;
    g_latchedAt = Millis{0};
    g_ready    = true;
    return ErrCode::OK;
}

void evaluate(const core::SystemState& snap, Millis now)
{
    // FAIL-SAFE: başlatılmadıysa HER ŞEY kilitli. Hesaplanamayan güvenlik
    // durumu "izin var" anlamına gelemez.
    if (!g_ready || g_cfg == nullptr)
    {
        g_mask = ILK_EMERGENCY_LATCHED;
        return;
    }

    // Akış doğrulaması ÖNCE ilerletilir. Ayrı bir çağrı noktası bırakılsaydı,
    // unutulması kuru çalışma korumasını SESSİZCE devre dışı bırakırdı
    // (TASK-031 Karar 4). Döngüsel bağımlılık yok: `flow` buraya geri
    // çağrı yapmaz, yalnızca durumunu bildirir.
    flow::evaluate(snap, now);
    setExternalInterlock(ILK_DRY_RUN, flow::latched());

    // Acil durum mandalı (TASK-032) veto zincirine buradan girer. Mandalı
    // SET etmek EmergencyStop'un, VETO etmek SafetyMonitor'un işidir —
    // tek veto noktası ilkesi böyle korunur.
    setExternalInterlock(ILK_EMERGENCY_LATCHED, emergency::latched());

    uint32_t mask = g_external;   // TASK-031 (kuru çalışma) + TASK-032 (mandal)
    mask |= evaluateLevel(snap);
    mask |= evaluateMaxRuntime();

    if (((mask & ILK_EMERGENCY_LATCHED) != 0u) &&
        ((g_prevMask & ILK_EMERGENCY_LATCHED) == 0u))
    {
        g_latchedAt = now;
    }

    logEdges(mask, g_prevMask);
    g_prevMask = mask;
    g_mask     = mask;
}

ErrCode permits(ActuatorId id)
{
    // FAIL-SAFE: geçersiz kimlik veya başlatılmamış modül → izin YOK.
    if (!g_ready) { return ErrCode::SAFETY_BLOCKED; }
    if (static_cast<uint8_t>(id) >= core::MAX_ACTUATORS) { return ErrCode::SAFETY_BLOCKED; }

    const uint32_t blocking = g_mask & masksFor(id);
    return (blocking == 0u) ? ErrCode::OK : reportedReason(blocking);
}

uint32_t interlocks() { return g_mask; }

void setExternalInterlock(Interlock bit, bool active)
{
    if (active) { g_external |= static_cast<uint32_t>(bit); }
    else        { g_external &= ~static_cast<uint32_t>(bit); }
}

core::ErrCode acknowledge(const core::SystemState& snap, Millis now)
{
    if (!g_ready || g_cfg == nullptr) { return ErrCode::SAFETY_BLOCKED; }

    // 1) YALNIZCA canlı fiziksel koşullar. Mandallar ve sayaçlar bilinçli
    //    olarak dışarıda: onlar zaten temizlenecek olanlardır ve kontrol
    //    edilseydi kendini engelleyen bir kilit oluşurdu.
    const uint32_t live = evaluateLevel(snap);
    if (live != ILK_NONE)
    {
        core::diag::log(core::LogLevel::WARNING, ErrCode::SAFETY_BLOCKED,
                        static_cast<int32_t>(live),
                        "onay reddedildi - su seviyesi hala uygun degil");
        return ErrCode::SAFETY_BLOCKED;
    }

    // 2-3) Mandalları temizle.
    flow::acknowledge();
    setExternalInterlock(ILK_DRY_RUN, false);
    (void)emergency::clear(0u);
    setExternalInterlock(ILK_EMERGENCY_LATCHED, false);

    // 4) Kilitleri yeniden hesapla — arayüz temizlenmiş durumu hemen görsün.
    evaluate(snap, now);
    return ErrCode::OK;
}

core::ErrCode publish(Millis now)
{
    (void)now;

    core::SafetyStatus out{};
    out.interlockMask     = g_mask;
    out.emergencyLatched  = ((g_mask & ILK_EMERGENCY_LATCHED) != 0u) ? 1u : 0u;
    out.emergencyReason   = reportedReason(g_mask);
    out.latchedAt         = g_latchedAt;

    return core::state::publishSafety(out);
}

} // namespace safety
} // namespace domain
