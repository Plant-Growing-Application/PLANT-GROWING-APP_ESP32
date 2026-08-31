#pragma once

// IP yapılandırma kararı — TASK-036
//
// ── DÜZELTİLEN MİMARİ HATA ──────────────────────────────────────────────────
//   Eski: _useDHCP = (Setting.IsServerMode == 0)
//         → DHCP tercihi AP/STA MODUNA bağlanmıştı. İki kavram ilgisiz.
//   Yeni: `config.network.ipMode ∈ {DHCP, STATIC}` — ayrı, bağımsız alan.
//
// Karar SAF bir fonksiyondur: girdi config, çıktı uygulanacak plan. Radyoya
// dokunmaz, log yazmaz — böylece TASK-064 tüm eksik-alan kombinasyonlarını
// donanımsız koşturabilir.

#include <stdint.h>

#include "core/Config.h"
#include "core/ErrorCodes.h"

namespace services {
namespace net {

/// Uygulanacak IP planı.
struct IpPlan
{
    uint32_t      ip;
    uint32_t      gateway;
    uint32_t      subnet;
    uint32_t      dns;
    bool          useStatic;
    core::ErrCode warning;   ///< `OK` değilse: istenen plan uygulanamadı
};

/// Statik yapılandırma eksiksiz mi?
///
/// IP, gateway ve subnet **zorunludur**. DNS eksik olabilir — gateway'e
/// düşülür (aşağıya bakın).
constexpr bool staticComplete(const core::NetworkConfig& n)
{
    return n.staticIp != 0u && n.gateway != 0u && n.subnet != 0u;
}

/// Config'ten uygulanacak planı türetir.
///
/// **Eksik statik alanda DHCP'ye düşülür ve `warning` doldurulur.** Sessizce
/// DHCP'ye düşmek, kullanıcının "statik IP ayarladım ama çalışmıyor"
/// sorusunu cevapsız bırakır — çağıran bu uyarıyı loglamalıdır.
///
/// **DNS boşsa gateway kullanılır:** DNS'siz bir statik yapılandırmada SNTP
/// çalışmaz ve saat hiç senkronize olmaz (TASK-040). Ev ağlarında gateway
/// neredeyse her zaman DNS'i de sunar.
constexpr IpPlan planFor(const core::NetworkConfig& n)
{
    return (n.ipMode != core::IpMode::STATIC)
               ? IpPlan{0u, 0u, 0u, 0u, false, core::ErrCode::OK}
           : staticComplete(n)
               ? IpPlan{n.staticIp, n.gateway, n.subnet,
                        (n.dns != 0u) ? n.dns : n.gateway, true, core::ErrCode::OK}
               : IpPlan{0u, 0u, 0u, 0u, false, core::ErrCode::NET_IP_CONFIG_INVALID};
}

} // namespace net
} // namespace services
