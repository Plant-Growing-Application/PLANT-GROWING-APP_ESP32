#pragma once

// Ağ FSM'inin İÇ durumu — TASK-035
//
// KATMAN NOTU: TASK-035'in dosya listesi bunu `domain/models/` altında,
// FSM'i ise `services/network/` altında gösteriyordu. Bu bir D1 İHLALİ
// üretirdi: servis (L2) katmanı domain (L3) başlığını include edemez.
// Ağ FSM'i bir altyapı politikasıdır ve servis katmanına aittir; durum
// modeli de sahibiyle aynı katmanda tutuldu.
//
// `core::NetworkStatus` (yayınlanan) ile bu yapı AYRIDIR. İç sayaçların ve
// zamanlayıcıların arayüze sızması gereksizdir; yayınlanan görünüm bundan
// TÜRETİLİR ve hangi iç bilginin yayınlanacağı bilinçli seçilir.
//
// SÜRE ÖLÇÜMÜ (ISSUE-012): bekleme bir SON TARİH olarak değil, "başlangıç
// anı + süre" olarak tutulur. `hasElapsed(now, retryFrom, retryDelayMs)`
// taşmada doğru çalışır; `now >= deadline` karşılaştırması çalışmaz.
//
// Tek istisna sunum: kullanıcı "8 sn sonra tekrar denenecek"
// görebilmeli. Sessiz bekleme kullanıcıya "bozuk" izlenimi verir — eski
// sistemin en çok şikâyet edilen davranışı buydu.

#include <stdint.h>

#include "core/SystemState.h"
#include "core/Time.h"
#include "services/network/NetworkEvents.h"

namespace services {
namespace net {

struct NetworkRuntime
{
    core::Millis stateSince;        ///< mevcut duruma ne zaman girildi
    core::Millis retryFrom;         ///< beklemenin BASLADIGI an (ISSUE-012 deseni)
    uint32_t     retryDelayMs;      ///< o andan itibaren beklenecek sure
    core::Millis disconnectedSince; ///< AP fallback süre ölçütü
    core::Millis connectedSince;
    core::Millis lastRssiAt;

    DisconnectClass lastClass;
    core::ErrCode                  lastError;
    core::NetState                 state;

    uint8_t attempt;       ///< backoff üssü
    uint8_t authFailures;  ///< kimlik hatası sayacı — yeni credential'da sıfırlanır
    uint8_t stopped;       ///< kimlik hatası sınırına ulaşıldı, deneme durdu
    uint8_t retryNow;      ///< kullanıcı "şimdi dene" dedi → backoff atlanır

    // ── KURULUM OTURUMU (TASK-038 / ISSUE-041) ─────────────────────────────
    // `provisioning`, cihazın kurulum AP'sini KİMLİK BİLGİSİ OLMADIĞI için
    // açtığı oturumu işaretler: ilk açılış veya "ağı unut" sonrası. AP
    // fallback bu oturumun parçası DEĞİLDİR — orada zaten çalışan bir
    // yapılandırma vardır ve yeniden başlatmak sorunu çözmez, döngü yaratır.
    core::Millis setupDoneAt;   ///< kurulumda IP alındığı an — nefes payı sayacı
    uint8_t provisioning;       ///< kurulum oturumu sürüyor
    uint8_t setupDone;          ///< kurulumda IP alındı, kontrollü reset bekliyor
    uint8_t rebootPosted;       ///< SYSTEM_RESTART komutu kuyruğa kondu

    void reset()
    {
        stateSince        = core::Millis{0};
        retryFrom         = core::Millis{0};
        retryDelayMs      = 0;
        disconnectedSince = core::Millis{0};
        connectedSince    = core::Millis{0};
        lastRssiAt        = core::Millis{0};
        lastClass         = DisconnectClass::UNKNOWN;
        lastError         = core::ErrCode::OK;
        state             = core::NetState::BOOT;
        setupDoneAt       = core::Millis{0};
        attempt           = 0;
        authFailures      = 0;
        stopped           = 0;
        retryNow          = 0;
        provisioning      = 0;
        setupDone         = 0;
        rebootPosted      = 0;
    }
};

} // namespace net
} // namespace services
