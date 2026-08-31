#include "services/network/ConnectionManager.h"

#include <string.h>

#include "core/Diagnostics.h"
#include "hal/SecretStore.h"
#include "hal/WifiRadio.h"
#include "services/ConfigService.h"
#include "services/network/IpConfig.h"

namespace services {
namespace net {
namespace conn {
namespace {

using core::ErrCode;
using core::Millis;

const core::Config* g_cfg          = nullptr;
Millis              g_attemptAt{0};
uint32_t            g_lastConnectMs = 0;
bool                g_attempting    = false;

} // namespace

core::ErrCode begin(const core::Config& cfg)
{
    g_cfg           = &cfg;
    g_attempting    = false;
    g_lastConnectMs = 0;
    return ErrCode::OK;
}

bool hasCredentials()
{
    return g_cfg != nullptr && g_cfg->network.ssid.length() > 0;
}

core::ErrCode beginConnect(Millis now)
{
    if (!hasCredentials()) { return ErrCode::NET_NO_CREDENTIALS; }

    // 1) IP planı — bağlantı BAŞLATILMADAN ÖNCE. Sonradan uygulamak etkisizdir.
    const IpPlan plan = planFor(g_cfg->network);
    if (plan.warning != ErrCode::OK)
    {
        // Sessizce DHCP'ye düşmüyoruz: kullanıcı "statik IP ayarladım ama
        // çalışmıyor" dediğinde cevabı log'da bulunmalı.
        core::diag::log(core::LogLevel::WARNING, plan.warning, 0,
                        "statik ip alanlari eksik - dhcp'ye dusuldu");
    }

    const ErrCode ipRc = plan.useStatic
                             ? hal::wifi::staStaticIp(plan.ip, plan.gateway, plan.subnet,
                                                      plan.dns)
                             : hal::wifi::staUseDhcp();
    if (ipRc != ErrCode::OK)
    {
        core::diag::log(core::LogLevel::WARNING, ipRc, 0, "ip yapilandirmasi uygulanamadi");
    }

    // 2) Şifre: SecretStore → yığın → radyo. Kullanımdan sonra SIFIRLANIR.
    char   pass[hal::WIFI_PASSWORD_MAX] = {0};
    size_t plen                         = sizeof(pass);
    if (hal::secrets::getWifiPassword(pass, plen) != ErrCode::OK)
    {
        pass[0] = '\0';   // açık ağ olabilir; şifresiz denenir
    }

    const ErrCode rc = hal::wifi::staConnect(g_cfg->network.ssid.c_str(), pass);

    // Yığında kalan bir kopya stack dump'ında görünebilir.
    memset(pass, 0, sizeof(pass));

    if (rc == ErrCode::OK)
    {
        g_attemptAt  = now;
        g_attempting = true;
    }
    return rc;
}

core::ErrCode abort()
{
    g_attempting = false;
    return hal::wifi::staDisconnect();
}

bool timedOut(Millis now)
{
    return g_attempting &&
           core::hasElapsed(now, g_attemptAt, core::millisecs(CONNECT_TIMEOUT_MS));
}

uint32_t     lastConnectMs()    { return g_lastConnectMs; }
core::Millis attemptStartedAt() { return g_attemptAt; }

void onConnected(Millis now)
{
    if (g_attempting)
    {
        g_lastConnectMs = core::elapsed(now, g_attemptAt).ms;
        g_attempting    = false;
    }
}

core::ErrCode setCredentials(const char* ssid, const char* password)
{
    if (ssid == nullptr || ssid[0] == '\0') { return ErrCode::CFG_VALIDATION_FAILED; }

    // Mevcut bağlantı temiz kapatılır; yarım kalmış geçiş radyoyu tanımsız
    // bırakır.
    (void)hal::wifi::staDisconnect();

    core::NetworkConfig n = g_cfg->network;
    if (!n.ssid.assign(ssid)) { return ErrCode::CFG_VALIDATION_FAILED; }

    const core::ConfigError e = services::config::updateNetwork(n);
    if (e.code != ErrCode::OK) { return e.code; }

    return hal::secrets::setWifiPassword((password != nullptr) ? password : "");
}

core::ErrCode forget()
{
    (void)hal::wifi::staDisconnect();

    core::NetworkConfig n = g_cfg->network;
    n.ssid.clear();
    (void)services::config::updateNetwork(n);

    const ErrCode rc = hal::secrets::clearWifiPassword();
    core::diag::log(core::LogLevel::WARNING, rc, 0, "kayitli ag silindi");
    return rc;
}

} // namespace conn
} // namespace net
} // namespace services
