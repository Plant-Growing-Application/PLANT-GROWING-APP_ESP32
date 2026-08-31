#include "services/network/ScanService.h"

#include <string.h>

#include "core/Diagnostics.h"
#include "hal/WifiRadio.h"

namespace services {
namespace net {
namespace scan {
namespace {

using core::ErrCode;
using core::Millis;

/// Tarama emniyet valfi — `SCAN_DONE` hiç gelmezse.
constexpr uint32_t SCAN_TIMEOUT_MS = 15000u;

ScanEntry g_results[MAX_RESULTS];
uint8_t   g_count     = 0;
ScanState g_state     = ScanState::IDLE;
Millis    g_startedAt{0};
Millis    g_doneAt{0};
bool      g_truncated = false;

/// Aynı SSID birden fazla kez görünebilir (mesh / repeater). En güçlü
/// sinyalli olan tutulur — kullanıcıya aynı ağı üç kez göstermek listeyi
/// okunmaz yapar.
///
/// @return kaydın yazılacağı indeks, veya `MAX_RESULTS` (yer yok)
uint8_t slotFor(const char* ssid, int8_t rssi)
{
    for (uint8_t i = 0; i < g_count; ++i)
    {
        if (strncmp(g_results[i].ssid, ssid, SSID_MAX) == 0)
        {
            return (rssi > g_results[i].rssi) ? i : MAX_RESULTS;
        }
    }
    return (g_count < MAX_RESULTS) ? g_count : MAX_RESULTS;
}

} // namespace

core::ErrCode begin()
{
    g_count     = 0;
    g_state     = ScanState::IDLE;
    g_truncated = false;
    return ErrCode::OK;
}

core::ErrCode start(Millis now)
{
    if (g_state == ScanState::RUNNING) { return ErrCode::OK; }

    // Gizli ağlar taranmaz (varsayılan): SSID'siz kayıtlar listeyi kirletir
    // ve kullanıcı zaten SSID'yi elle girebilir.
    const ErrCode rc = hal::wifi::scanStart(false);
    if (rc != ErrCode::OK)
    {
        g_state = ScanState::FAILED;
        core::diag::log(core::LogLevel::WARNING, rc, 0, "tarama baslatilamadi");
        return rc;
    }

    g_state     = ScanState::RUNNING;
    g_startedAt = now;
    g_truncated = false;
    return ErrCode::OK;
}

void onScanDone(Millis now, uint8_t count)
{
    g_count     = 0;
    g_truncated = false;

    for (uint8_t i = 0; i < count; ++i)
    {
        char    ssid[SSID_MAX] = {0};
        int8_t  rssi           = 0;
        uint8_t channel        = 0;
        uint8_t enc            = 0;

        if (!hal::wifi::scanResult(i, ssid, sizeof(ssid), rssi, channel, enc)) { continue; }
        if (ssid[0] == '\0') { continue; }   // gizli ağ

        const uint8_t slot = slotFor(ssid, rssi);
        if (slot >= MAX_RESULTS)
        {
            if (g_count >= MAX_RESULTS) { g_truncated = true; }
            continue;
        }

        strncpy(g_results[slot].ssid, ssid, SSID_MAX - 1);
        g_results[slot].ssid[SSID_MAX - 1] = '\0';
        g_results[slot].rssi               = rssi;
        g_results[slot].channel            = channel;
        g_results[slot].encType            = enc;

        if (slot == g_count) { ++g_count; }
    }

    // Radyo tarafındaki belleği serbest bırak — sonuçlar artık BİZİM
    // tamponumuzda. Eski sistem de `scanDelete()` çağırıyordu; bu doğru
    // davranış korundu, ancak kopyalama ondan önce yapılıyor.
    hal::wifi::scanRelease();

    g_state  = ScanState::DONE;
    g_doneAt = now;
}

void onScanFailed(Millis now)
{
    (void)now;
    hal::wifi::scanRelease();
    g_state = ScanState::FAILED;
    core::diag::raise(ErrCode::NET_SCAN_FAILED);
}

void tick(Millis now)
{
    if (g_state != ScanState::RUNNING) { return; }

    if (core::hasElapsed(now, g_startedAt, core::millisecs(SCAN_TIMEOUT_MS)))
    {
        // Olay hiç gelmedi. `RUNNING`'de sonsuza kadar kalmak, arayüzün
        // "taranıyor..." göstergesini kalıcı hâle getirirdi.
        onScanFailed(now);
    }
}

ScanState        state()   { return g_state; }
uint8_t          count()   { return (g_state == ScanState::DONE) ? g_count : 0u; }
const ScanEntry* results() { return g_results; }
bool             truncated() { return g_truncated; }

uint32_t ageMs(Millis now)
{
    return (g_state == ScanState::DONE) ? core::elapsed(now, g_doneAt).ms : 0u;
}

} // namespace scan
} // namespace net
} // namespace services
