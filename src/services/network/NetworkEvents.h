#pragma once

// Kopma nedeninin POLİTİKA sınıflandırması — TASK-034
//
// Bu dosya `hal/WifiRadio.h`'ın taşıdığı **ham** `wifi_err_reason_t`
// değerini, FSM ve backoff'un kullanabileceği bir **davranış sınıfına**
// çevirir.
//
// Neden sürücüde değil: D6 — "sürücüler iş kuralı içermez". "Yanlış şifrede
// 3 denemeden sonra dur" bir politikadır; hangi ham kodun yanlış şifre
// anlamına geldiği de öyle (AP üreticileri farklı kod döndürebilir).
//
// Ham kod `WifiEventRecord.reasonRaw` içinde KORUNUR: teşhis için gerekli,
// karar için değil.

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "hal/WifiRadio.h"

namespace services {
namespace net {

/// Kopmanın davranış sınıfı — TASK-037 buna göre farklı backoff uygular.
enum class DisconnectClass : uint8_t
{
    UNKNOWN      = 0,  ///< sınıflandırılamadı → backoff ile devam
    AUTH_FAILED  = 1,  ///< yanlış şifre → SINIRLI deneme; sonsuz deneme anlamsız
    AP_NOT_FOUND = 2,  ///< kapsama dışı → backoff ile devam
    LINK_LOST    = 3,  ///< sinyal koptu → ilk deneme HIZLI
    REJECTED     = 4,  ///< AP meşgul/reddetti → backoff ile devam
};

/// Ham `wifi_err_reason_t` → davranış sınıfı.
///
/// Değerler `esp_wifi_types.h`'tan **okunarak** doğrulandı, varsayılmadı.
/// Sayısal sabitler kullanılıyor çünkü bu dosya ESP-IDF başlığını include
/// etmez (L2 katmanı sürücü başlığına bağımlı olmamalı).
constexpr DisconnectClass classify(uint16_t raw)
{
    // 202 AUTH_FAIL · 15 4WAY_HANDSHAKE_TIMEOUT · 204 HANDSHAKE_TIMEOUT
    // 2 AUTH_EXPIRE · 14 MIC_FAILURE
    // 4-yollu el sıkışma zaman aşımı pratikte YANLIŞ ŞİFRE demektir: AP
    // bulunmuş, kimlik doğrulama aşamasında takılınmıştır.
    return (raw == 202u || raw == 15u || raw == 204u || raw == 2u || raw == 14u)
               ? DisconnectClass::AUTH_FAILED
         // 201 NO_AP_FOUND
         : (raw == 201u) ? DisconnectClass::AP_NOT_FOUND
         // 200 BEACON_TIMEOUT · 4 ASSOC_EXPIRE · 8 ASSOC_LEAVE
         : (raw == 200u || raw == 4u || raw == 8u) ? DisconnectClass::LINK_LOST
         // 5 ASSOC_TOOMANY · 203 ASSOC_FAIL · 205 CONNECTION_FAIL · 6 NOT_AUTHED
         : (raw == 5u || raw == 203u || raw == 205u || raw == 6u) ? DisconnectClass::REJECTED
                                                                  : DisconnectClass::UNKNOWN;
}

/// Sınıfın neden koduna çevrimi — loglama ve `NetworkStatus.lastError` için.
constexpr core::ErrCode errorOf(DisconnectClass c)
{
    return (c == DisconnectClass::AUTH_FAILED)  ? core::ErrCode::NET_AUTH_FAILED
         : (c == DisconnectClass::AP_NOT_FOUND) ? core::ErrCode::NET_AP_NOT_FOUND
                                                : core::ErrCode::NET_DISCONNECTED;
}

// --- Derleme zamanı doğrulama ----------------------------------------------

static_assert(classify(202u) == DisconnectClass::AUTH_FAILED, "202 = AUTH_FAIL");
static_assert(classify(15u) == DisconnectClass::AUTH_FAILED, "15 = 4WAY_HANDSHAKE_TIMEOUT");
static_assert(classify(201u) == DisconnectClass::AP_NOT_FOUND, "201 = NO_AP_FOUND");
static_assert(classify(200u) == DisconnectClass::LINK_LOST, "200 = BEACON_TIMEOUT");
static_assert(classify(999u) == DisconnectClass::UNKNOWN, "bilinmeyen kod UNKNOWN olmali");
// Yanlış şifre ASLA "sinyal koptu" sayılmamalı: ikincisi HIZLI yeniden
// deneme yapar ve sonsuz döngü üretir.
static_assert(classify(202u) != DisconnectClass::LINK_LOST,
              "kimlik hatasi link kopmasi olarak siniflandirilamaz");

} // namespace net
} // namespace services
