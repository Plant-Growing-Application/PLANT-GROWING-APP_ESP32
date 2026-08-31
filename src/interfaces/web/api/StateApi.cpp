// GET /api/state — TASK-043
//
// WebSocket push birincil yoldur (TASK-046). Bu uç nokta YALNIZCA ilk
// yükleme ve WS kuramayan istemciler için durur.
//
// Eski frontend her 600 ms'de `GET /api/sensors` yapıyordu: sürekli HTTP
// isteği, TCP el sıkışması, JSON üretimi ve CPU. O polling KALDIRILDI.

#include <ESPAsyncWebServer.h>

#include "core/StateStore.h"
#include "interfaces/web/ApiRoutes.h"
#include "interfaces/web/HttpResponses.h"
#include "interfaces/web/RequestValidation.h"
#include "interfaces/web/StateJson.h"
#include "interfaces/web/WsProtocol.h"

namespace interfaces {
namespace web {
namespace api {

void registerState(AsyncWebServer& server)
{
    server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) { return; }

        core::SystemState snap{};
        (void)core::state::snapshot(snap);

        // Önceden boyutlandırılmış tampon (§14.6): dinamik `String`
        // birleştirme heap'i parçalar.
        static char json[ws::STATE_JSON_MAX];
        const size_t n = writeStateJson(snap, json, sizeof(json));
        if (n == 0)
        {
            sendError(req, core::ErrCode::WEB_PAYLOAD_TOO_LARGE);
            return;
        }
        sendJson(req, 200, json);
    });
}

} // namespace api
} // namespace web
} // namespace interfaces
