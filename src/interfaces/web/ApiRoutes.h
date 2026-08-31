#pragma once

// REST API rota kaydı — TASK-043 / TASK-044
//
// Her grup kendi dosyasında kayıt olur; `registerAll()` tek giriş noktasıdır.
//
// ── ORTAK KURALLAR ──────────────────────────────────────────────────────────
//   1. Komutlar DOĞRUDAN `domain/` çağırmaz — `CommandQueue`'ya gider.
//      Komut yolu TEKTİR ve AsyncTCP bağlamından iş yapılmaz.
//   2. Her yanıt `HttpResponses`'ın tek hata şemasını kullanır.
//   3. Yazma uç noktaları `requireAuth()`'tan geçer.
//   4. Kurulum modunda (parola yok) YALNIZCA `/api/setup/password` açıktır.

class AsyncWebServer;

namespace interfaces {
namespace web {
namespace api {

void registerState(AsyncWebServer& server);        ///< TASK-043
void registerDiagnostics(AsyncWebServer& server);  ///< TASK-043
void registerSystem(AsyncWebServer& server);       ///< TASK-043
void registerAuth(AsyncWebServer& server);         ///< TASK-042
void registerConfig(AsyncWebServer& server);       ///< TASK-044
void registerNetwork(AsyncWebServer& server);      ///< TASK-044
void registerHistory(AsyncWebServer& server);      ///< TASK-059

/// Tüm rota gruplarını kaydeder.
void registerAll(AsyncWebServer& server);

} // namespace api
} // namespace web
} // namespace interfaces
