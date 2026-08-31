#pragma once

// Ortak HTTP yanıt ve hata şeması — TASK-041
//
// ── TEK HATA ŞEMASI ─────────────────────────────────────────────────────────
//     { "error": { "code": 2306, "message": "SAFETY_BLOCKED", "field": "..." } }
//
// Tüm endpoint'ler bunu kullanır. Farklı endpoint'lerin farklı şekilli hata
// döndürmesi istemci hatalarının ana kaynağıdır — TASK-039'daki "tarama
// sürerken farklı şema" hatası bunun somut bedeliydi.
//
// ── ASYNCTCP BAĞLAM KURALI (§14.6) ──────────────────────────────────────────
// Buradaki hiçbir yardımcı flash okumaz, beklemez veya donanıma dokunmaz.
// Yanıtlar bellekte kurulur ve tek seferde gönderilir.

#include <ESPAsyncWebServer.h>

#include "core/ErrorCodes.h"

namespace interfaces {
namespace web {

/// İstek gövdesi üst sınırı (§14.5). Aşan istek `WEB_PAYLOAD_TOO_LARGE`
/// ile reddedilir — sınırsız gövde heap'i tüketip cihazı düşürür.
constexpr size_t MAX_BODY_BYTES = 4096;

/// `ErrCode` → okunabilir ad. Sunum katmanının işi; `core/` içinde metin
/// tutulmaz (Diagnostics deseni).
const char* errorName(core::ErrCode c);

/// `ErrCode` → HTTP durum kodu.
uint16_t httpStatusFor(core::ErrCode c);

/// Ortak hata yanıtı gönderir.
///
/// @param field ilgili alan adı; yoksa `nullptr`
void sendError(AsyncWebServerRequest* req, core::ErrCode code, const char* field = nullptr);

/// Hazır bir JSON gövdesini gönderir (`application/json`).
void sendJson(AsyncWebServerRequest* req, uint16_t status, const char* json);

/// `{"ok":true}` — gövde gerektirmeyen başarılı işlemler için.
void sendOk(AsyncWebServerRequest* req);

} // namespace web
} // namespace interfaces
