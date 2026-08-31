#include "services/TimeService.h"

#include <lwip/apps/sntp.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "core/Diagnostics.h"
#include "core/StateStore.h"

namespace services {
namespace timesvc {
namespace {

using core::ErrCode;
using core::Millis;

/// 2023-01-01. Bundan küçük bir epoch, saatin hiç ayarlanmadığını gösterir:
/// ESP32 açılışta 1970'ten sayar ve bu değer "geçerli zaman" sanılamaz.
constexpr int64_t SANE_EPOCH_MIN = 1672531200LL;

constexpr const char* NTP_PRIMARY   = "pool.ntp.org";
constexpr const char* NTP_SECONDARY = "time.google.com";

const core::Config* g_cfg = nullptr;
Millis   g_bootAt{0};
Millis   g_lastSyncAt{0};
Millis   g_lastAttemptAt{0};
uint32_t g_firstSyncMs = 0;
bool     g_valid       = false;
bool     g_started     = false;
bool     g_ready       = false;

int64_t readEpoch()
{
    timeval tv{};
    gettimeofday(&tv, nullptr);
    return static_cast<int64_t>(tv.tv_sec);
}

/// SNTP'yi başlatır. Callback tabanlıdır — bloklayan `getLocalTime()`
/// beklemesi YAPILMAZ; sonuç bir sonraki `tick()`'te görülür.
void startSntp()
{
    if (g_started) { return; }

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, const_cast<char*>(NTP_PRIMARY));
    sntp_setservername(1, const_cast<char*>(NTP_SECONDARY));
    sntp_init();
    g_started = true;
}

} // namespace

core::ErrCode begin(const core::Config& cfg)
{
    g_cfg          = &cfg;
    g_valid        = false;
    g_started      = false;
    g_firstSyncMs  = 0;
    g_ready        = true;

    applyTimezone();
    return ErrCode::OK;
}

void applyTimezone()
{
    if (g_cfg == nullptr) { return; }

    // POSIX TZ dizesi — DST kuralları DAHİL. Eski sistemdeki sabit
    // `GMT+3` ofseti yaz saatinde çizelgeyi bir saat kaydırıyordu.
    const char* tz = g_cfg->system.timezone.c_str();
    if (tz != nullptr && tz[0] != '\0')
    {
        setenv("TZ", tz, 1);
        tzset();
    }
}

void tick(Millis now, bool netConnected)
{
    if (!g_ready) { return; }
    if (g_bootAt.v == 0u) { g_bootAt = now; }

    // Ağ yokken SNTP denemesi radyo ve CPU harcar, sonucu bellidir.
    if (!netConnected)
    {
        if (g_started) { sntp_stop(); g_started = false; }
        return;
    }

    startSntp();

    const bool wasValid = g_valid;
    const int64_t e     = readEpoch();
    g_valid             = (e >= SANE_EPOCH_MIN);

    if (g_valid && !wasValid)
    {
        g_lastSyncAt  = now;
        g_firstSyncMs = core::elapsed(now, g_bootAt).ms;
        core::diag::clear(ErrCode::TIME_NOT_SYNCED);
        core::diag::log(core::LogLevel::INFO, ErrCode::OK,
                        static_cast<int32_t>(g_firstSyncMs), "saat senkronize edildi");
        return;
    }

    if (g_valid)
    {
        // Periyodik yeniden senkronizasyon: kristal kayması saatte bir
        // düzeltilir. SNTP arka planda kendi poll'ünü yapar; burada yalnızca
        // damgayı tazeliyoruz.
        if (core::hasElapsed(now, g_lastSyncAt, core::millisecs(RESYNC_PERIOD_MS)))
        {
            g_lastSyncAt = now;
        }
        return;
    }

    // Henüz senkronize olmadı. Sessiz kalmıyoruz: çizelgeler duraklamış
    // durumda ve kullanıcı nedenini görebilmeli.
    if (core::hasElapsed(now, g_lastAttemptAt, core::millisecs(FIRST_SYNC_RETRY_MS)))
    {
        g_lastAttemptAt = now;
        core::diag::raise(ErrCode::TIME_NOT_SYNCED);
    }
}

bool valid() { return g_valid; }

core::EpochSeconds epoch()
{
    // Geçersizken SAHTE DEĞER DÖNDÜRÜLMEZ.
    return g_valid ? core::EpochSeconds{readEpoch()} : core::EPOCH_INVALID;
}

uint32_t firstSyncMs() { return g_firstSyncMs; }

core::ErrCode publish(Millis now)
{
    (void)now;

    core::TimeStatus s{};
    s.epoch      = epoch();
    s.lastSyncAt = g_lastSyncAt;
    s.valid      = g_valid ? 1u : 0u;

    return core::state::publishTime(s);
}

} // namespace timesvc
} // namespace services
