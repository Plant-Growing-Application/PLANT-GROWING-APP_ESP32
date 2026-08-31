// GET /api/diagnostics — TASK-043
//
// Aktif hatalar, son olaylar ve sayaçlar. Eski sistemde arıza teşhisi için
// seri porta bakmak gerekiyordu; artık arayüzden görülebiliyor.
//
// Kayıtlar SERBEST METİN TAŞIMAZ (`LogRecord` 12 bayt: zaman + kod + seviye
// + bağlam). Kod → metin çevirisi istemcinin işidir; gömülü bir sistemde
// kilobaytlarca metin saklamak savunulamaz.

#include <ESPAsyncWebServer.h>

#include "interfaces/web/ApiRoutes.h"
#include "interfaces/web/HttpResponses.h"
#include "interfaces/web/RequestValidation.h"
#include "interfaces/web/StateJson.h"

namespace interfaces {
namespace web {
namespace api {

namespace {
constexpr size_t DIAG_JSON_MAX = 2048;
}

void registerDiagnostics(AsyncWebServer& server)
{
    server.on("/api/diagnostics", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) { return; }

        static char json[DIAG_JSON_MAX];
        const size_t n = writeDiagnosticsJson(json, sizeof(json));
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
