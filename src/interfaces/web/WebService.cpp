#include "interfaces/web/WebService.h"

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "core/Diagnostics.h"
#include "hal/FileStore.h"
#include "interfaces/web/ApiRoutes.h"
#include "interfaces/web/HttpResponses.h"
#include "interfaces/web/WsProtocol.h"

namespace interfaces {
namespace web {
namespace {

using core::ErrCode;

AsyncWebServer g_server(HTTP_PORT);
bool           g_listening = false;
bool           g_fsReady   = false;

} // namespace

AsyncWebServer& server() { return g_server; }

core::ErrCode begin()
{
    const bool fsReady = hal::fs::isMounted();

    if (fsReady)
    {
        // `serveStatic` gzip'li dosyayı (`.gz`) kendisi bulur ve
        // `Content-Encoding: gzip` ekler. Elle dosya araması yazmak, AsyncTCP
        // bağlamında dosya sistemi taraması yapmak (YASAK) ve tekerleği
        // yeniden icat etmek olurdu.
        //
        // `index.html` uzun önbelleğe ALINMAZ: aksi hâlde firmware
        // güncellemesinden sonra kullanıcı ESKİ sayfayı görür ve
        // "güncelleme çalışmadı" der.
        g_server.serveStatic("/", LittleFS, "/")
            .setDefaultFile("index.html")
            .setCacheControl("no-cache");

        // Hash'li isim taşıyan varlıklar uzun önbelleğe alınabilir.
        g_server.serveStatic("/assets/", LittleFS, "/assets/")
            .setCacheControl("public, max-age=31536000, immutable");
    }
    else
    {
        core::diag::raise(ErrCode::STORAGE_FS_MOUNT_FAILED);
    }

    // Bilinmeyen yol: dosya sistemi yoksa SESSİZ 404 değil, açık neden.
    g_server.onNotFound([fsReady](AsyncWebServerRequest* req) {
        sendError(req, fsReady ? ErrCode::CFG_NOT_FOUND : ErrCode::STORAGE_FS_MOUNT_FAILED);
    });

    api::registerAll(g_server);
    ws::attach(g_server);

    // DİNLEMEYE BURADA BAŞLANMAZ — bkz. `start()`.
    g_fsReady = fsReady;
    return ErrCode::OK;
}

core::ErrCode start()
{
    if (g_listening) { return ErrCode::OK; }

    g_server.begin();
    g_listening = true;

    // Eski sistemde `begin()` dönüşü hiç kontrol edilmiyordu; sunucunun
    // gerçekten dinlediği bilinmiyordu.
    core::diag::log(core::LogLevel::INFO, ErrCode::OK, HTTP_PORT,
                    g_fsReady ? "web sunucusu dinliyor"
                              : "web sunucusu dinliyor - dosya sistemi YOK");
    return ErrCode::OK;
}

bool listening() { return g_listening; }

void tick(core::Millis now) { ws::tick(now); }

} // namespace web
} // namespace interfaces
