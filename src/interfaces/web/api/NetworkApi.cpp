// Ağ uç noktaları — TASK-044
//
//   POST /api/network/scan     — taramayı başlatır
//   GET  /api/network/scan     — durumu sorar
//   POST /api/network/forget   — "ağı unut"
//   POST /api/network/retry    — backoff'u atla, şimdi dene
//
// ── DÜZELTİLEN HATA: ARA DURUM ──────────────────────────────────────────────
// Eski sistem tarama sürerken `202 {"status":"scanning"}` döndürüyordu.
// Frontend bunu dizi sanıp `forEach` çağırıyor → hata → "Ağ taraması
// yapılamadı!" → **ilk tıklama HER ZAMAN başarısız**.
//
// Burada HER İKİ uç nokta da HER DURUMDA aynı şemayı döndürür:
//     { status, age, truncated, networks[] }
// `networks` her zaman dizidir, boş olsa bile. Hiçbir durumda 202 dönülmez:
// durum GÖVDEDEDİR, HTTP kodunda değil.

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "core/Command.h"
#include "core/CommandQueue.h"
#include "interfaces/web/ApiRoutes.h"
#include "interfaces/web/HttpResponses.h"
#include "interfaces/web/RequestValidation.h"
#include "interfaces/web/StateJson.h"
#include "services/network/NetworkFsm.h"
#include "services/network/ScanService.h"

namespace interfaces {
namespace web {
namespace api {
namespace {

using core::ErrCode;

constexpr size_t SCAN_JSON_MAX = 1536;

void sendScanState(AsyncWebServerRequest* req)
{
    static char json[SCAN_JSON_MAX];
    const size_t n = writeScanJson(core::Millis{millis()}, json, sizeof(json));
    if (n == 0) { sendError(req, ErrCode::WEB_PAYLOAD_TOO_LARGE); return; }
    sendJson(req, 200, json);   // HER ZAMAN 200 — durum gövdede
}

} // namespace

void registerNetwork(AsyncWebServer& server)
{
    server.on("/api/network/scan", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) { return; }

        // Tarama zaten çalışıyorsa yeniden başlatılmaz; `start()` bunu
        // kendisi ele alır ve `OK` döner.
        (void)services::net::scan::start(core::Millis{millis()});

        // Başlatma sonucu bile AYNI şemayla döner: istemci tek bir
        // ayrıştırıcı yazar.
        sendScanState(req);
    });

    server.on("/api/network/scan", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) { return; }
        sendScanState(req);
    });

    server.on("/api/network/forget", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) { return; }

        core::Command cmd{};
        cmd.issuedAt = core::Millis{millis()};
        cmd.source   = core::CommandSource::WEB;
        cmd.type     = core::CommandType::NETWORK_FORGET;

        if (core::cmdq::post(cmd) != core::CommandResult::ACCEPTED)
        {
            sendError(req, ErrCode::WEB_BUSY);
            return;
        }
        sendOk(req);
    });

    // "Şimdi dene": kullanıcı sorunu düzelttiğinde 60 saniye beklemek
    // zorunda kalmamalı. Backoff'u VE kimlik hatası durdurmasını atlar.
    server.on("/api/network/retry", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) { return; }
        services::net::fsm::requestRetryNow();
        sendOk(req);
    });
}

void registerAll(AsyncWebServer& server)
{
    registerAuth(server);
    registerState(server);
    registerDiagnostics(server);
    registerSystem(server);
    registerConfig(server);
    registerNetwork(server);
    registerHistory(server);
}

} // namespace api
} // namespace web
} // namespace interfaces
