#pragma once

// JSON gövdeli istek yardımcısı — TASK-060 (entegrasyon düzeltmesi)
//
// ── NEDEN KÜTÜPHANENİN `AsyncCallbackJsonWebHandler`'I KULLANILMIYOR ────────
// `ESPAsyncWebServer/AsyncJson.cpp` içeriği tamamen
// `#if ASYNC_JSON_SUPPORT == 1` koruması altında ve bu bayrak
// `__has_include("ArduinoJson.h")` ile belirleniyor. PlatformIO her
// kütüphaneyi KENDİ include yollarıyla derlediği için ArduinoJson,
// ESPAsyncWebServer'ın derleme birimi için görünmüyor → `ASYNC_JSON_SUPPORT`
// 0 oluyor → sınıfın kurucusu HİÇ ÜRETİLMİYOR.
//
// Sonuç bir LİNK hatasıydı ve ancak boot wiring bağlandığında ortaya çıktı:
// o ana kadar tüm web zinciri ölü koddu ve linker onu atıyordu.
//
// Burada gövdeyi kendimiz topluyoruz ve ArduinoJson'ı KENDİ derleme
// birimimizde kullanıyoruz — orada zaten görünür.
//
// ── GÖVDE TAMPONU `_tempObject` İÇİNDE ──────────────────────────────────────
// İstek nesnesinin yıkıcısı `_tempObject`'i `free()` ediyor
// (`WebRequest.cpp:109`), dolayısıyla sızıntı yok ve istekler arası
// karışma da yok — tampon isteğe aittir, paylaşılan bir statik değil.

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#include <functional>

namespace interfaces {
namespace web {

/// Gövde ayrıştırıldıktan sonra çağrılır.
using JsonBodyFn = std::function<void(AsyncWebServerRequest*, JsonDocument&)>;

/// JSON gövdeli bir yol kaydeder.
///
/// Gövde `MAX_BODY_BYTES`'ı aşarsa istek `WEB_PAYLOAD_TOO_LARGE` ile
/// reddedilir — sınırsız gövde heap'i tüketip cihazı düşürür (§14.5).
void onJsonBody(AsyncWebServer& server, const char* path, WebRequestMethodComposite method,
                JsonBodyFn fn);

} // namespace web
} // namespace interfaces
