#pragma once

// İstek doğrulama ve yetki ara katmanı — TASK-044
//
// ── İSTEMCİYE GÜVENİLMEZ ────────────────────────────────────────────────────
// Her yazma isteğinin gövdesi `ConfigValidation` (TASK-014) üzerinden geçer.
// İstemci tarafı doğrulama bir KOLAYLIKTIR, güvenlik önlemi değildir:
// `curl` ile gönderilen bir istek istemci kodunu hiç çalıştırmaz.

#include <ESPAsyncWebServer.h>

#include "core/ErrorCodes.h"

namespace interfaces {
namespace web {

/// İstek yetkili mi? Değilse 401 gönderir ve `false` döner.
///
/// Token `Authorization: Bearer <token>` başlığında veya `token` sorgu
/// parametresinde taşınabilir.
bool requireAuth(AsyncWebServerRequest* req);

/// Sistem kurulum modunda mı? (`true` ise yalnızca kurulum uç noktaları açık)
bool setupMode();

/// İstek KURULUM AP'si üzerinden mi geldi?
///
/// Kurulum uç noktası (`POST /api/setup/password`) yalnızca AP üzerinden
/// açıktır (TASK-042 tasarım kaydı). Aksi hâlde parolası olmayan bir cihaz
/// ev ağına bağlıysa (firmware yükseltmesi, kısmi NVS bozulması) ağdaki
/// HERKES ilk parolayı belirleyip cihazı sahiplenebilirdi.
///
/// Ölçüt: istemci IP'si SoftAP alt ağında mı (`192.168.4.x`).
bool fromSetupAp(AsyncWebServerRequest* req);

/// `"192.168.1.10"` → ham IPv4. Geçersizse `false`.
bool parseIp(const char* text, uint32_t& out);

} // namespace web
} // namespace interfaces
