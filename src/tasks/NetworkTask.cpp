// net task — TASK-035 / TASK-040
//
// İNCE SARMALAYICI: FSM ve zaman servisi kendi modüllerinde, `now`
// parametreli ve host'ta test edilebilir.
//
// Bu task ağ radyosunun TEK SAHİBİDİR (ARCHITECTURE §6.1, P2). Core 0'a
// sabitlenmiştir — Wi-Fi/lwIP yığını da orada çalışır.
//
// BAĞIMSIZLIK: bu task tamamen kilitlense bile aktüatör kontrolü ve
// güvenlik kilitleri çalışmaya devam eder. `net` yalnızca `StateStore`'a
// yazar; `app_core` ondan hiçbir şey beklemez (ARCHITECTURE §16.3).

#include <Arduino.h>

#include "TaskConfig.h"
#include "TaskRunner.h"
#include "core/Diagnostics.h"
#include "services/ConfigService.h"
#include "services/CropService.h"
#include "services/TimeService.h"
#include "hal/WifiRadio.h"
#include "interfaces/web/AuthService.h"
#include "interfaces/web/WebService.h"
#include "interfaces/ui/UiService.h"
#include "services/network/NetworkFsm.h"
#include "services/network/SoftApManager.h"

namespace tasks {

void networkTaskEntry(void*)
{
    TaskRunner runner(core::TaskId::NET, core::TaskClass::NETWORK, core::millisecs(100));
    runner.begin();

    const core::Config& cfg = services::config::get();

    core::ErrCode rc = services::net::fsm::begin(cfg);
    if (rc != core::ErrCode::OK)
    {
        // Ağ kurulamadıysa sistem DURMAZ: sensörler okunur, güvenlik çalışır,
        // OLED üzerinden yönetilebilir (ARCHITECTURE P4 — fail-degraded).
        core::diag::log(core::LogLevel::ERROR, rc, 0, "ag altyapisi baslatilamadi");
    }

    rc = services::timesvc::begin(cfg);
    if (rc != core::ErrCode::OK)
    {
        core::diag::log(core::LogLevel::WARNING, rc, 0, "zaman servisi baslatilamadi");
    }

    // Rotalar burada KAYDEDILIR; sunucu HENÜZ DİNLEMEYE BAŞLAMAZ.
    //
    // `AsyncServer::begin()` lwIP'in TCP/IP thread'ini kullanır ve radyo
    // `WIFI_MODE_NULL` iken o thread HİÇ BAŞLATILMAMIŞ olur → panik.
    // Dinleme, radyo bir moda geçtikten sonra döngüde başlatılır.
    (void)interfaces::web::auth::begin();
    (void)interfaces::web::begin();

    for (;;)
    {
        const core::Millis now{millis()};

        services::net::fsm::tick(now);

        // ── SUNUCUYU RADYO HAZIR OLUNCA BAŞLAT ─────────────────────────────
        // FSM ilk tick'te radyoyu bir moda alır (credential varsa STA, yoksa
        // kurulum AP'si). O andan itibaren lwIP yığını ayaktadır.
        //
        // Sunucu AP ve STA fark etmeksizin başlar: kurulum tam olarak AP
        // modunda yapılır ve yalnızca STA bağlıyken başlamak cihazın hiç
        // yapılandırılamaması demektir (TASK-041 Karar 5).
        if (!interfaces::web::listening() &&
            hal::wifi::mode() != hal::wifi::RadioMode::OFF)
        {
            (void)interfaces::web::start();
        }

        // Kurulum AP bilgisini arayuze BILDIR.
        //
        // `ui` radyoya dokunamaz (P2); bu degerler `net` tarafindan
        // dolduruluyor. Bu cagri EKSIKTI: `setApInfo()` tanimliydi ama
        // HICBIR YERDEN cagrilmiyordu, dolayisiyla OLED'de kurulum SSID'si
        // ve sifresi HIC gorunmuyordu — kullanicinin cihaza girmesinin tek
        // yolu buydu.
        if (services::net::softap::active())
        {
            interfaces::ui::setApInfo(services::net::softap::ssid(),
                                      services::net::softap::password());
        }

        services::timesvc::tick(now, services::net::fsm::state() == core::NetState::CONNECTED);
        (void)services::timesvc::publish(now);

        // Gelişim dönemi ilerlemesi (TASK-068). BURADA, `app_core`'da DEĞİL:
        // config'e yazan tek bağlam bu task'tır (web API de buradan koşar) ve
        // `app_core` config'i yalnızca okur. Saatte bir kez iş yapar; zaman
        // geçersizken hiçbir şey yapmaz.
        services::crop::tick(now);

        // Kopan WS istemcilerini temizle ve telemetri yayınla. Eski sistemde
        // `cleanupClients()` `loop()` içindeydi; artık ağ task'inin işi.
        interfaces::web::tick(now);
        interfaces::web::auth::tick(now);

        runner.endCycle();
    }
}

} // namespace tasks
