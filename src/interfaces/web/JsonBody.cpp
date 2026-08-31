#include "interfaces/web/JsonBody.h"

#include <stdlib.h>
#include <string.h>

#include "interfaces/web/HttpResponses.h"

namespace interfaces {
namespace web {
namespace {

using core::ErrCode;

/// İstek başına gövde tamponu. `_tempObject` içinde yaşar ve isteğin
/// yıkıcısı tarafından `free()` edilir.
struct BodyBuf
{
    uint16_t len;
    uint16_t cap;
    char     data[1];   ///< değişken uzunluk — `malloc` ile boyutlandırılır
};

/// Gövde parçalarını biriktirir. **Captureless** — her istekte yeni bir
/// `std::function` üretilmez.
void accumulate(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index,
                size_t total)
{
    if (req == nullptr) { return; }

    if (index == 0u)
    {
        // Sınır KONTROLÜ ÖNCE: `total` başlıktan geliyor ve istemci
        // uydurabilir; tahsisten önce reddediyoruz.
        if (total == 0u || total > MAX_BODY_BYTES) { return; }

        BodyBuf* b = static_cast<BodyBuf*>(malloc(sizeof(BodyBuf) + total));
        if (b == nullptr) { return; }
        b->len          = 0;
        b->cap          = static_cast<uint16_t>(total);
        req->_tempObject = b;
    }

    BodyBuf* b = static_cast<BodyBuf*>(req->_tempObject);
    if (b == nullptr) { return; }

    // Gerçek gelen veri `total`ı aşarsa (bozuk/kötü niyetli istek) kırpılır.
    const size_t room = (b->cap > b->len) ? static_cast<size_t>(b->cap - b->len) : 0u;
    const size_t n    = (len < room) ? len : room;
    if (n > 0u)
    {
        memcpy(b->data + b->len, data, n);
        b->len = static_cast<uint16_t>(b->len + n);
    }
}

} // namespace

void onJsonBody(AsyncWebServer& server, const char* path, WebRequestMethodComposite method,
                JsonBodyFn fn)
{
    server.on(
        path, method,
        // Gövde tamamlandıktan SONRA çağrılır.
        [fn](AsyncWebServerRequest* req) {
            BodyBuf* b = static_cast<BodyBuf*>(req->_tempObject);
            if (b == nullptr || b->len == 0u)
            {
                sendError(req, ErrCode::WEB_PAYLOAD_TOO_LARGE);
                return;
            }

            JsonDocument doc;
            if (deserializeJson(doc, b->data, b->len))
            {
                sendError(req, ErrCode::WEB_INVALID_REQUEST);
                return;
            }
            fn(req, doc);
        },
        nullptr,
        accumulate);
}

} // namespace web
} // namespace interfaces
