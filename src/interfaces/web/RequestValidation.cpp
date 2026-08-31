#include "interfaces/web/RequestValidation.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "interfaces/web/AuthService.h"
#include "interfaces/web/HttpResponses.h"

namespace interfaces {
namespace web {

using core::ErrCode;

bool setupMode() { return !auth::configured(); }

bool requireAuth(AsyncWebServerRequest* req)
{
    if (req == nullptr) { return false; }

    const core::Millis now{millis()};

    // `Authorization: Bearer <token>` tercih edilen yol.
    if (req->hasHeader("Authorization"))
    {
        const String v = req->header("Authorization");
        if (v.startsWith("Bearer "))
        {
            if (auth::validate(v.c_str() + 7, now)) { return true; }
        }
    }

    // Sorgu parametresi: tarayıcının WebSocket API'si başlık göndermeye
    // izin vermediği için WS ile aynı yolu HTTP'de de açık tutuyoruz.
    if (req->hasParam("token") && auth::validate(req->getParam("token")->value().c_str(), now))
    {
        return true;
    }

    sendError(req, ErrCode::WEB_UNAUTHORIZED);
    return false;
}

bool fromSetupAp(AsyncWebServerRequest* req)
{
    if (req == nullptr || req->client() == nullptr) { return false; }

    // `192.168.4.0/24` — `SoftApManager::AP_IP` ile aynı alt ağ.
    // `getRemoteAddress()` ham (little-endian) IPv4 döndürür.
    const uint32_t ip = req->client()->getRemoteAddress();
    return (ip & 0x00FFFFFFu) == 0x0004A8C0u;
}

bool parseIp(const char* text, uint32_t& out)
{
    if (text == nullptr) { return false; }

    unsigned a = 0, b = 0, c = 0, d = 0;
    if (sscanf(text, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) { return false; }
    if (a > 255u || b > 255u || c > 255u || d > 255u) { return false; }

    // Ham gösterim `WiFi`/`IPAddress` ile aynı bayt sırasında (little-endian).
    out = a | (b << 8) | (c << 16) | (d << 24);
    return true;
}

} // namespace web
} // namespace interfaces
