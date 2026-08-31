#include "domain/EmergencyStop.h"

#include "core/Diagnostics.h"
#include "domain/ActuatorManager.h"
#include "hal/NvsStore.h"

namespace domain {
namespace emergency {
namespace {

using core::ErrCode;
using core::Millis;

/// NVS anahtarları — `NVS_KEY_NAME_MAX_SIZE = 16` (sonlandırıcı dahil).
constexpr const char* KEY_LATCH = "emg";
constexpr const char* KEY_BOOTN = "emgboot";

constexpr uint32_t MAGIC = 0x454D4731u;   // "EMG1"

LatchRecord g_rec{};
uint32_t    g_bootCount = 0;
bool        g_ready     = false;

/// Mandalı kalıcı hâle getirir. Röleler ZATEN güvenli olduktan sonra çağrılır.
void persist()
{
    const ErrCode e = hal::nvsstore::setBlob(hal::NS_SYSTEM, KEY_LATCH, &g_rec,
                                             sizeof(g_rec));
    if (e != ErrCode::OK)
    {
        // Kalıcılık bir iyileştirmedir; güvenlik RAM mandalında ve kapalı
        // rölelerdedir. Yazma başarısızlığı acil durumu geçersiz kılmaz.
        core::diag::log(core::LogLevel::WARNING, e, 0, "acil durum mandali kalici yazilamadi");
    }
}

} // namespace

core::ErrCode begin()
{
    g_rec  = LatchRecord{};
    g_ready = true;

    // Boot sayacı — mandalın hangi oturumda oluştuğunu ayırt etmek için.
    if (hal::nvsstore::getU32(hal::NS_SYSTEM, KEY_BOOTN, g_bootCount) != ErrCode::OK)
    {
        g_bootCount = 0;
    }
    g_bootCount++;
    (void)hal::nvsstore::setU32(hal::NS_SYSTEM, KEY_BOOTN, g_bootCount);

    LatchRecord stored{};
    size_t      len = sizeof(stored);
    if (hal::nvsstore::getBlob(hal::NS_SYSTEM, KEY_LATCH, &stored, len) == ErrCode::OK &&
        len == sizeof(stored) && stored.magic == MAGIC && stored.latched != 0u)
    {
        // Mandal güç kesintisinden SAĞ ÇIKTI. Kendiliğinden temizlenmez.
        g_rec = stored;
        core::diag::raise(ErrCode::SAFETY_EMERGENCY_LATCHED,
                          static_cast<int32_t>(stored.reason));
        core::diag::log(core::LogLevel::CRITICAL, ErrCode::SAFETY_EMERGENCY_LATCHED,
                        static_cast<int32_t>(stored.bootCount),
                        "onceki oturumdan acil durum mandali suruyor");
    }

    return ErrCode::OK;
}

void trigger(ErrCode reason, uint8_t source, Millis now)
{
    // Yeniden girişte etkisiz: İLK neden korunur. Sonraki tetikleyiciler
    // asıl nedeni gizlememelidir.
    if (g_rec.latched != 0u) { return; }

    // 1) RAM mandalı — mikrosaniyeler.
    g_rec.magic     = MAGIC;
    g_rec.reason    = reason;
    g_rec.latched   = 1u;
    g_rec.source    = source;
    g_rec.uptimeMs  = now.v;
    g_rec.bootCount = g_bootCount;

    // 2) Röleler GÜVENLİ. İş esasen burada biter.
    actuators::forceAllOff(reason, now);

    // 3) Kayıt.
    core::diag::raise(ErrCode::SAFETY_EMERGENCY_LATCHED, static_cast<int32_t>(reason));
    core::diag::log(core::LogLevel::CRITICAL, reason, static_cast<int32_t>(source),
                    "ACIL DURUM mandallandi");

    // 4) Kalıcılık — bloklayan flash yazması, her şey güvenliyken.
    if (g_ready) { persist(); }
}

bool          latched()          { return g_rec.latched != 0u; }
ErrCode       reason()           { return latched() ? g_rec.reason : ErrCode::OK; }
uint32_t      latchedUptimeMs()  { return g_rec.uptimeMs; }
uint32_t      latchedBootCount() { return g_rec.bootCount; }

core::ErrCode clear(uint32_t blockingMask)
{
    if (g_rec.latched == 0u) { return ErrCode::SAFETY_EMERGENCY_LATCHED; }

    // Koşullar düzelmeden temizleme REDDEDİLİR. Aksi hâlde operatör onay
    // verir, pompa açılır ve aynı arıza tekrarlar.
    if (blockingMask != 0u)
    {
        core::diag::log(core::LogLevel::WARNING, ErrCode::SAFETY_BLOCKED,
                        static_cast<int32_t>(blockingMask),
                        "acil durum temizleme reddedildi - kosullar duzelmemis");
        return ErrCode::SAFETY_BLOCKED;
    }

    core::diag::log(core::LogLevel::WARNING, g_rec.reason, 0,
                    "acil durum operator onayiyla temizlendi");
    core::diag::clear(ErrCode::SAFETY_EMERGENCY_LATCHED);

    g_rec.latched = 0u;
    g_rec.reason  = ErrCode::OK;

    if (g_ready)
    {
        (void)hal::nvsstore::eraseKey(hal::NS_SYSTEM, KEY_LATCH);
    }

    // Aşım sayaçları da sıfırlanır: temizlenmeyen bir sayaç, mandal
    // kalkar kalkmaz kilidi yeniden kurar.
    actuators::clearMaxRunViolations();

    return ErrCode::OK;
}

} // namespace emergency
} // namespace domain
