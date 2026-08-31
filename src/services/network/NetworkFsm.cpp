#include "services/network/NetworkFsm.h"

#include <esp_random.h>
#include <stdio.h>
#include <string.h>

#include "core/Diagnostics.h"
#include "core/StateStore.h"
#include "hal/WifiRadio.h"
#include "services/network/ConnectionManager.h"
#include "services/network/RetryPolicy.h"
#include "services/network/ScanService.h"
#include "services/network/SoftApManager.h"

namespace services {
namespace net {
namespace fsm {
namespace {

using core::ErrCode;
using core::Millis;
using core::NetState;

constexpr uint32_t RSSI_PERIOD_MS = 1000u;   ///< her döngüde okumak gereksiz

const core::Config* g_cfg = nullptr;
NetworkRuntime      g_rt;
bool                g_ready = false;
uint8_t             g_forgetRequested = 0;

const char* nameOf(NetState s)
{
    return (s == NetState::BOOT)        ? "BOOT"
         : (s == NetState::AP_ONLY)     ? "AP_ONLY"
         : (s == NetState::CONNECTING)  ? "CONNECTING"
         : (s == NetState::CONNECTED)   ? "CONNECTED"
         : (s == NetState::BACKOFF)     ? "BACKOFF"
                                        : "AP_FALLBACK";
}

/// Durum geçişi. Her geçiş INFO loglanır — ağ sorunlarının teşhisi buna dayanır.
void enter(NetState s, Millis now)
{
    if (g_rt.state == s) { return; }

    core::diag::log(core::LogLevel::INFO, ErrCode::OK, static_cast<int32_t>(s), nameOf(s));
    g_rt.state      = s;
    g_rt.stateSince = now;
}

/// Bekleme doldu mu?
///
/// ISSUE-012 DESENI: son tarih saklanip `now >= deadline` karsilastirmasi
/// YAPILMAZ — unsigned tasmada kirilir. Beklemenin BASLADIGI an ve SURE
/// saklanir, `hasElapsed()` gecen sureyi tasma-guvenli hesaplar.
///
/// "Simdi dene" komutu beklemeyi ATLAR.
bool retryDue(Millis now)
{
    return g_rt.retryNow != 0u ||
           core::hasElapsed(now, g_rt.retryFrom, core::millisecs(g_rt.retryDelayMs));
}

/// Backoff süresini hesaplar ve beklemeyi kurar.
void scheduleRetry(Millis now)
{
    if (retry::shouldStop(g_rt.lastClass, g_rt.authFailures))
    {
        g_rt.stopped = 1u;
        core::diag::raise(ErrCode::NET_AUTH_FAILED, g_rt.authFailures);
        core::diag::log(core::LogLevel::ERROR, ErrCode::NET_AUTH_FAILED, g_rt.authFailures,
                        "kimlik hatasi siniri - deneme durduruldu");
        return;
    }

    // Rastgelelik DIŞARIDAN: `retry::delayFor` saf kalır ve host'ta
    // deterministik test edilebilir.
    const uint8_t  rnd   = static_cast<uint8_t>(esp_random() & 0xFFu);
    const uint32_t delay = retry::delayFor(g_rt.lastClass, g_rt.attempt, rnd);

    g_rt.retryFrom    = now;
    g_rt.retryDelayMs = delay;
    if (g_rt.attempt < 200u) { ++g_rt.attempt; }
}

/// Bir denemeyi başlatır.
void tryConnect(Millis now)
{
    if (conn::beginConnect(now) == ErrCode::OK) { enter(NetState::CONNECTING, now); }
    else                                        { enter(NetState::BACKOFF, now); }
}

/// Kopma işlendi: sınıflandır, sayaçları güncelle, yeniden deneme planla.
void handleDisconnect(uint16_t reasonRaw, Millis now)
{
    g_rt.lastClass = classify(reasonRaw);
    g_rt.lastError = errorOf(g_rt.lastClass);

    if (g_rt.lastClass == DisconnectClass::AUTH_FAILED && g_rt.authFailures < 255u)
    {
        ++g_rt.authFailures;
    }

    if (g_rt.state == NetState::CONNECTED)
    {
        g_rt.disconnectedSince = now;
        core::diag::log(core::LogLevel::WARNING, g_rt.lastError, reasonRaw, "baglanti koptu");
    }

    scheduleRetry(now);
    enter((g_rt.state == NetState::AP_FALLBACK) ? NetState::AP_FALLBACK : NetState::BACKOFF,
          now);
}

void handleGotIp(Millis now)
{
    conn::onConnected(now);

    // Cihazin adresini SERI PORTA yaz. Kullanicinin bunu router arayuzunden
    // aramasi gerekmemeli; OLED yoksa veya okunmuyorsa tek kaynak burasi.
    {
        const uint32_t ip = hal::wifi::localIp();
        char line[64];
        snprintf(line, sizeof(line), "BAGLANDI  ->  http://%u.%u.%u.%u",
                 static_cast<unsigned>(ip & 0xFFu),
                 static_cast<unsigned>((ip >> 8) & 0xFFu),
                 static_cast<unsigned>((ip >> 16) & 0xFFu),
                 static_cast<unsigned>((ip >> 24) & 0xFFu));
        core::diag::log(core::LogLevel::INFO, ErrCode::OK, 0, line);
    }
    g_rt.connectedSince = now;
    g_rt.lastError      = ErrCode::OK;
    core::diag::clear(ErrCode::NET_DISCONNECTED);
    core::diag::clear(ErrCode::NET_AUTH_FAILED);
    enter(NetState::CONNECTED, now);
}

void consumeEvents(Millis now)
{
    hal::wifi::WifiEventRecord ev;
    while (hal::wifi::popEvent(ev))
    {
        switch (ev.event)
        {
            case hal::wifi::WifiEvent::STA_GOT_IP:
                handleGotIp(now);
                break;

            case hal::wifi::WifiEvent::STA_DISCONNECT:
            case hal::wifi::WifiEvent::STA_LOST_IP:
                handleDisconnect(ev.reasonRaw, now);
                break;

            case hal::wifi::WifiEvent::SCAN_DONE:
                scan::onScanDone(now, ev.detail);
                break;

            case hal::wifi::WifiEvent::STA_CONNECTED:
                // AP'ye bağlanıldı ama IP HENÜZ YOK. `CONNECTED` sayılmaz:
                // bu noktada web sunucusu dinlemeye başlarsa hiçbir adreste
                // erişilemez ve SNTP başarısız olur.
                break;

            case hal::wifi::WifiEvent::STA_STARTED:
            case hal::wifi::WifiEvent::AP_STARTED:
            case hal::wifi::WifiEvent::AP_STOPPED:
            case hal::wifi::WifiEvent::AP_CLIENT_JOIN:
            case hal::wifi::WifiEvent::AP_CLIENT_LEFT:
                break;

            case hal::wifi::WifiEvent::NONE:
            default:
                core::diag::log(core::LogLevel::WARNING, ErrCode::NET_DISCONNECTED,
                                static_cast<int32_t>(ev.event), "tanimsiz wifi olayi");
                break;
        }
    }
}

/// AP'yi gereken modla birlikte açar.
void openAp(Millis now)
{
    (void)hal::wifi::setMode(conn::hasCredentials() ? hal::wifi::RadioMode::AP_STA
                                                    : hal::wifi::RadioMode::AP);
    (void)softap::start(now);
}

void publish(Millis now)
{
    core::NetworkStatus s{};

    s.state          = g_rt.state;
    s.lastError      = g_rt.lastError;
    s.retryCount     = g_rt.attempt;
    // Sunum icin son tarih TURETILIR; ic hesap "baslangic + sure" uzerinden yapilir.
    s.nextRetryAt    = Millis{g_rt.retryFrom.v + g_rt.retryDelayMs};
    s.connectedSince = g_rt.connectedSince;
    s.apActive       = softap::active() ? 1u : 0u;
    s.apClients      = softap::active() ? softap::clientCount() : 0u;
    s.usingStaticIp  = (g_cfg->network.ipMode == core::IpMode::STATIC) ? 1u : 0u;

    // ŞİFRE BURADA YOK ve asla olmayacak (ARCHITECTURE §8.2).
    (void)s.ssid.assign(g_cfg->network.ssid.c_str());

    if (g_rt.state == NetState::CONNECTED)
    {
        s.ipv4    = hal::wifi::localIp();
        s.gateway = hal::wifi::gatewayIp();
        s.subnet  = hal::wifi::subnetMask();
        s.dns     = hal::wifi::dnsIp();
        s.rssi    = hal::wifi::rssi();
    }

    hal::wifi::macAddress(s.mac);
    (void)core::state::publishNetwork(s);
    (void)now;
}

} // namespace

core::ErrCode begin(const core::Config& cfg)
{
    g_cfg = &cfg;
    g_rt.reset();

    ErrCode rc = hal::wifi::begin();
    if (rc != ErrCode::OK) { return rc; }

    rc = conn::begin(cfg);
    if (rc != ErrCode::OK) { return rc; }

    (void)softap::begin();
    (void)scan::begin();

    g_ready = true;
    return ErrCode::OK;
}

void tick(Millis now)
{
    if (!g_ready) { return; }

    consumeEvents(now);
    scan::tick(now);

    // "Agi unut" — komut yolundan gelen bayrak. Is BURADA yapilir: radyo ve
    // flash bu task'a ait. Eski hali komut yolunda SESSIZCE DUSUYORDU.
    if (g_forgetRequested != 0u)
    {
        g_forgetRequested = 0u;
        (void)conn::forget();
        onCredentialsChanged();
        g_rt.disconnectedSince = now;
        openAp(now);
        enter(NetState::AP_ONLY, now);
    }

    switch (g_rt.state)
    {
        case NetState::BOOT:
            // Credential yoksa `CONNECTING`'e HİÇ girilmez: boş SSID ile
            // bağlanma denemesi anlamsız bir başarısızlık döngüsüdür.
            if (!conn::hasCredentials())
            {
                g_rt.disconnectedSince = now;
                openAp(now);
                enter(NetState::AP_ONLY, now);
            }
            else
            {
                g_rt.disconnectedSince = now;
                (void)hal::wifi::setMode(hal::wifi::RadioMode::STA);
                tryConnect(now);
            }
            break;

        case NetState::CONNECTING:
            // Emniyet valfi: olay hiç gelmezse sonsuza kadar burada kalınmaz.
            if (conn::timedOut(now))
            {
                (void)conn::abort();
                g_rt.lastClass = DisconnectClass::UNKNOWN;
                g_rt.lastError = ErrCode::NET_CONNECT_TIMEOUT;
                core::diag::log(core::LogLevel::WARNING, ErrCode::NET_CONNECT_TIMEOUT, 0,
                                "baglanti zaman asimi - emniyet valfi");
                scheduleRetry(now);
                enter(NetState::BACKOFF, now);
            }
            break;

        case NetState::CONNECTED:
            // Sayaç sıfırlama STABİLİTE bekler: bağlantı kurulup 1 sn sonra
            // kopuyorsa bu "başarılı" sayılmamalı, yoksa backoff hiç devreye
            // girmez ve sistem sonsuz hızlı deneme döngüsüne girer.
            if (g_rt.attempt != 0u &&
                core::hasElapsed(now, g_rt.connectedSince, core::millisecs(retry::STABLE_MS)))
            {
                g_rt.attempt      = 0;
                g_rt.authFailures = 0;
                g_rt.stopped      = 0;
            }
            // AP açıksa ve artık gerekmiyorsa kapat.
            if (softap::active() && softap::canCloseNow(now, true))
            {
                (void)softap::stop();
                (void)hal::wifi::setMode(hal::wifi::RadioMode::STA);
            }
            break;

        case NetState::BACKOFF:
            if (softap::shouldFallback(now, g_rt.disconnectedSince))
            {
                openAp(now);
                enter(NetState::AP_FALLBACK, now);
                break;
            }
            if (g_rt.stopped == 0u && retryDue(now))
            {
                g_rt.retryNow = 0u;
                tryConnect(now);
            }
            break;

        case NetState::AP_ONLY:
            // Kullanıcı web arayüzünden credential girerse `onCredentialsChanged()`
            // bizi buradan çıkarır.
            if (conn::hasCredentials())
            {
                (void)hal::wifi::setMode(hal::wifi::RadioMode::AP_STA);
                tryConnect(now);
            }
            break;

        case NetState::AP_FALLBACK:
            // AP açık, STA denemesi ARKA PLANDA sürer. Ağ geri gelince
            // `STA_GOT_IP` olayı bizi `CONNECTED`'a taşır.
            if (g_rt.stopped == 0u && conn::hasCredentials() && retryDue(now))
            {
                g_rt.retryNow = 0u;
                (void)conn::beginConnect(now);
                scheduleRetry(now);
            }
            break;

        default:
            // Tanımsız durum: sessizce yok saymak FSM'i olayın gerçekleşmediği
            // varsayımıyla bırakır.
            core::diag::log(core::LogLevel::WARNING, ErrCode::NET_DISCONNECTED,
                            static_cast<int32_t>(g_rt.state), "tanimsiz fsm durumu");
            enter(NetState::BACKOFF, now);
            break;
    }

    // RSSI saniyede bir — her 100 ms'de okumak radyoya gereksiz sorgudur ve
    // değer o hızda anlamlı değişmez.
    if (core::hasElapsed(now, g_rt.lastRssiAt, core::millisecs(RSSI_PERIOD_MS)))
    {
        g_rt.lastRssiAt = now;
        publish(now);
    }
}

void requestForget()
{
    g_forgetRequested = 1u;
}

void requestRetryNow()
{
    g_rt.retryNow     = 1u;
    g_rt.stopped      = 0u;
    g_rt.attempt      = 0u;
    g_rt.retryFrom    = Millis{0};
    g_rt.retryDelayMs = 0u;
}

void onCredentialsChanged()
{
    g_rt.authFailures = 0u;
    g_rt.stopped      = 0u;
    g_rt.attempt      = 0u;
    g_rt.retryFrom    = Millis{0};
    g_rt.retryDelayMs = 0u;
    g_rt.lastClass    = DisconnectClass::UNKNOWN;
}

core::NetState        state()   { return g_rt.state; }
const NetworkRuntime& runtime() { return g_rt; }

} // namespace fsm
} // namespace net
} // namespace services
