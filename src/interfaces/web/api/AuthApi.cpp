// Kimlik doğrulama uç noktaları — TASK-042
//
//   POST /api/setup/password   — İLK parola; yalnızca kurulum modunda
//   POST /api/auth/login       — token alır
//   POST /api/auth/logout      — oturumu düşürür
//   POST /api/auth/password    — parola değiştirir (mevcut parolayı doğrular)
//   GET  /api/auth/status      — kurulum modunda mıyız? (yetki GEREKMEZ)
//
// ── PAROLA GÖVDEDE, YANITTA ASLA ────────────────────────────────────────────
// Parola sorgu parametresiyle DEĞİL gövdeyle gönderilir: sorgu dizeleri
// sunucu erişim loglarına ve tarayıcı geçmişine yazılır.
//
// ── HTTPS YOK ───────────────────────────────────────────────────────────────
// Parola ağ üzerinde açık gider (§14.4). Bilinçli ve belgelenmiş kısıt;
// cihaz yerel ağ cihazıdır. `/api/auth/status` bunu istemciye bildirir ki
// arayüz uyarıyı gösterebilsin.

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <stdio.h>

#include "core/Diagnostics.h"
#include "interfaces/web/ApiRoutes.h"
#include "interfaces/web/AuthService.h"
#include "interfaces/web/HttpResponses.h"
#include "interfaces/web/JsonBody.h"
#include "interfaces/web/RequestValidation.h"

namespace interfaces {
namespace web {
namespace api {
namespace {

using core::ErrCode;

/// JSON gövdeli bir POST işleyicisi kaydeder.
///
/// Kütüphanenin `AsyncCallbackJsonWebHandler` sınıfı KULLANILMIYOR:
/// ArduinoJson onun derleme birimine görünmediği için sınıfın kurucusu hiç
/// üretilmiyor ve LİNK HATASI veriyor (bkz. `JsonBody.h`).
void onJsonPost(AsyncWebServer& server, const char* path, JsonBodyFn fn)
{
    onJsonBody(server, path, HTTP_POST, fn);
}

} // namespace

void registerAuth(AsyncWebServer& server)
{
    // Yetki GEREKMEZ: istemci giriş ekranını çizmeden önce hangi modda
    // olduğunu bilmek zorunda.
    server.on("/api/auth/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        char body[128];
        snprintf(body, sizeof(body),
                 "{\"setupMode\":%s,\"secure\":false}",
                 setupMode() ? "true" : "false");
        // `secure:false` — HTTPS yok. Arayüz bunu görüp uyarı gösterir.
        sendJson(req, 200, body);
    });

    // İLK PAROLA. Yalnızca kurulum modunda çalışır; `setupPassword()` kendi
    // içinde de bunu doğrular (iki bağımsız engel).
    onJsonPost(server, "/api/setup/password", [](AsyncWebServerRequest* req, JsonDocument& json) {
        if (!setupMode())
        {
            sendError(req, ErrCode::WEB_UNAUTHORIZED);
            return;
        }

        // YALNIZCA KURULUM AP'si (TASK-042). Parolası olmayan bir cihaz ev
        // ağına bağlıysa, bu kontrol olmadan ağdaki herkes ilk parolayı
        // belirleyip cihazı sahiplenebilirdi.
        if (!fromSetupAp(req))
        {
            core::diag::log(core::LogLevel::WARNING, ErrCode::WEB_UNAUTHORIZED, 0,
                            "kurulum istegi AP disindan reddedildi");
            sendError(req, ErrCode::WEB_UNAUTHORIZED);
            return;
        }
        const char* pass = json["password"] | "";
        const ErrCode rc = auth::setupPassword(pass);
        if (rc != ErrCode::OK) { sendError(req, rc, "password"); return; }
        sendOk(req);
    });

    onJsonPost(server, "/api/auth/login", [](AsyncWebServerRequest* req, JsonDocument& json) {
        char token[auth::TOKEN_HEX_LEN + 1] = {0};
        const ErrCode rc =
            auth::login(json["password"] | "", core::Millis{millis()}, token, sizeof(token));

        if (rc != ErrCode::OK)
        {
            // Kaba kuvvet kilidi devredeyse de aynı yanıt döner: hangi
            // nedenle reddedildiğini söylemek saldırgana bilgi verir.
            sendError(req, ErrCode::WEB_UNAUTHORIZED);
            return;
        }

        char body[128];
        snprintf(body, sizeof(body), "{\"token\":\"%s\",\"ttlMs\":%u}", token,
                 static_cast<unsigned>(auth::SESSION_TTL_MS));
        sendJson(req, 200, body);
    });

    server.on("/api/auth/logout", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (req->hasParam("token"))
        {
            auth::logout(req->getParam("token")->value().c_str());
        }
        sendOk(req);
    });

    onJsonPost(server, "/api/auth/password", [](AsyncWebServerRequest* req, JsonDocument& json) {
        if (!requireAuth(req)) { return; }

        const ErrCode rc = auth::changePassword(json["current"] | "", json["next"] | "");
        if (rc != ErrCode::OK) { sendError(req, rc, "password"); return; }

        // Parola değişti → tüm oturumlar düştü (`changePassword` içinde).
        // İstemci yeniden giriş yapmak zorunda; bu bir hata değil, amaçtır.
        sendOk(req);
    });
}

} // namespace api
} // namespace web
} // namespace interfaces
