#include "interfaces/web/WebService.h"

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include <string.h>

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

    // ── WEB VARLIKLARI YÜKLENMİŞ Mİ ────────────────────────────────────────
    //
    // Dosya sistemi mount olmuş olabilir ama İÇİ BOŞ olabilir: firmware
    // (`pio run -t upload`) ve web varlıkları (`pio run -t uploadfs`) AYRI
    // yüklenir ve ikincisi kolayca unutulur. Ayrıca bir mount hatasından
    // sonra bölüm biçimlendirilirse varlıklar SİLİNİR.
    //
    // Bu durumda kullanıcı "IP'ye giriyorum ama açılmıyor" der ve nedenini
    // anlamasının hiçbir yolu olmaz — 404 JSON'u bir açıklama değildir.
    const bool assetsReady = fsReady && hal::fs::exists("/index.html.gz");

    if (fsReady && !assetsReady)
    {
        core::diag::log(core::LogLevel::ERROR, ErrCode::CFG_NOT_FOUND, 0,
                        "WEB VARLIKLARI YOK — 'pio run -t uploadfs' calistirin");
    }

    // Bilinmeyen yol: SESSİZ 404 değil, ne yapılacağını söyleyen açık yanıt.
    g_server.onNotFound([fsReady, assetsReady](AsyncWebServerRequest* req) {
        // Tarayıcıdan gelen bir SAYFA isteğine JSON dönmek, kullanıcıya
        // hiçbir şey anlatmaz. İnsan okunabilir bir sayfa döndürüyoruz.
        const bool wantsPage = (req->method() == HTTP_GET) &&
                               (strncmp(req->url().c_str(), "/api", 4) != 0);

        if (wantsPage && !assetsReady)
        {
            req->send(503, "text/html; charset=utf-8",
                      "<!doctype html><meta charset=utf-8>"
                      "<meta name=viewport content='width=device-width,initial-scale=1'>"
                      "<style>body{font:16px system-ui;background:#101418;color:#e6edf3;"
                      "margin:0;padding:24px}code{background:#182028;padding:2px 6px;"
                      "border-radius:4px}h1{font-size:19px}</style>"
                      "<h1>Web arayuzu yuklenmemis</h1>"
                      "<p>Cihaz calisiyor ve bu yaniti <b>o</b> uretti — yani ag ve "
                      "sunucu saglam. Eksik olan yalnizca arayuz dosyalari.</p>"
                      "<p>Bilgisayarda proje klasorunde:</p>"
                      "<p><code>python tools/build_assets.py</code><br>"
                      "<code>pio run -t uploadfs</code></p>"
                      "<p>Firmware ve web arayuzu <b>ayri</b> yuklenir; ikincisi "
                      "atlandiginda bu sayfayi gorursunuz.</p>");
            return;
        }

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
