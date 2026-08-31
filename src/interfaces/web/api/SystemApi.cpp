// Sistem uç noktaları — TASK-043
//
//   POST /api/system/restart          — onay gerekmez (geri dönüşü var)
//   POST /api/system/factory-reset    — AÇIK ONAY zorunlu
//   POST /api/system/emergency-stop   — onay GEREKMEZ, gecikmesi tehlikeli
//   POST /api/system/emergency-clear  — koşul kontrolü ZATEN var (TASK-032)
//
// ── KOMUTLAR KUYRUĞA GİDER ──────────────────────────────────────────────────
// Hiçbiri `domain/` fonksiyonunu doğrudan çağırmaz. AsyncTCP bağlamından
// aktüatör sürmek veya flash yazmak yasaktır (§14.6); eski sistemin en
// tehlikeli deseni tam olarak buydu.

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <string.h>

#include "core/Command.h"
#include "core/CommandQueue.h"
#include "core/Diagnostics.h"
#include "interfaces/web/ApiRoutes.h"
#include "interfaces/web/HttpResponses.h"
#include "interfaces/web/RequestValidation.h"

namespace interfaces {
namespace web {
namespace api {
namespace {

using core::Command;
using core::CommandSource;
using core::CommandType;
using core::ErrCode;

/// Fabrika ayarlarına dönüş için beklenen onay dizesi.
///
/// Tek tıkla silinebilen bir yapılandırma, er ya da geç yanlışlıkla silinir.
constexpr const char* RESET_CONFIRM = "FACTORY_RESET";

/// Komutu kuyruğa koyar ve sonucu ortak şemayla döndürür.
void postCommand(AsyncWebServerRequest* req, CommandType type, int32_t param, uint8_t target)
{
    Command cmd{};
    cmd.issuedAt = core::Millis{millis()};
    cmd.source   = CommandSource::WEB;
    cmd.type     = type;
    cmd.param    = param;
    cmd.target   = target;

    const core::CommandResult r = core::cmdq::post(cmd);
    if (r != core::CommandResult::ACCEPTED)
    {
        // Sessiz yutma yok: kuyruk doluysa istemci bunu görür.
        sendError(req, ErrCode::WEB_BUSY);
        return;
    }
    sendOk(req);
}

} // namespace

void registerSystem(AsyncWebServer& server)
{
    server.on("/api/system/restart", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) { return; }
        postCommand(req, CommandType::SYSTEM_RESTART, 0, 0);
    });

    // ACİL DURDURMA: onay diyaloğu YOK. Onay istemek, tam da gerektiği anda
    // bir saniye kaybettirir. Garantili yol kuyruğu tamamen atlar (TASK-008),
    // böylece kuyruk doluyken bile ulaşır.
    server.on("/api/system/emergency-stop", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) { return; }
        core::cmdq::postEmergencyStop(CommandSource::WEB, ErrCode::SAFETY_EMERGENCY_LATCHED);
        core::diag::log(core::LogLevel::CRITICAL, ErrCode::SAFETY_EMERGENCY_LATCHED, 0,
                        "web uzerinden acil durdurma");
        sendOk(req);
    });

    // Temizleme: koşulların düzelmiş olması `safety::acknowledge()` içinde
    // kontrol edilir (TASK-032). Burada ek kontrol yapılmaz — güvenlik
    // kararının iki yerde olması, ikisinin ayrışmasıyla biter.
    server.on("/api/system/emergency-clear", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) { return; }
        postCommand(req, CommandType::EMERGENCY_CLEAR, 0, 0);
    });

    // FABRİKA AYARLARI: açık onay ZORUNLU.
    server.on("/api/system/factory-reset", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) { return; }

        const bool confirmed =
            req->hasParam("confirm") &&
            strcmp(req->getParam("confirm")->value().c_str(), RESET_CONFIRM) == 0;

        if (!confirmed)
        {
            sendError(req, ErrCode::WEB_INVALID_REQUEST, "confirm");
            return;
        }
        postCommand(req, CommandType::FACTORY_RESET, 0, 0);
    });
}

} // namespace api
} // namespace web
} // namespace interfaces
