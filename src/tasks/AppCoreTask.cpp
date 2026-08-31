// app_core task — TASK-033
//
// İNCE SARMALAYICI: karar mantığının tamamı `domain/AppCore` içindedir ve
// FreeRTOS bilmez. Burada yalnızca zaman kaynağı ve döngü iskeleti var.
//
// Bu task sistemin EN YÜKSEK öncelikli task'ıdır (4). Mevcut projede en
// yüksek öncelik loglama task'ındaydı — güvenlik kararları loglamanın
// arkasında bekliyordu. Bu ters çevrilmiştir (ARCHITECTURE §6.3).

#include <Arduino.h>

#include "TaskConfig.h"
#include "TaskRunner.h"
#include "core/Diagnostics.h"
#include "domain/AppCore.h"
#include "services/ConfigService.h"

namespace tasks {

void appCoreTaskEntry(void*)
{
    TaskRunner runner(core::TaskId::APP_CORE, core::TaskClass::CONTROL,
                      core::millisecs(100));
    runner.begin();

    const core::ErrCode rc = domain::appcore::begin(services::config::get());
    if (rc != core::ErrCode::OK)
    {
        // Güvenlik zinciri kurulamadıysa döngüye girmek TEHLİKELİDİR:
        // `SafetyMonitor` başlatılmamışken `permits()` her aktüatör için ret
        // döndürür (fail-safe), ama bu sessiz bir kilitlenme olur.
        // Durumu CRITICAL kaydediyoruz; röleler Boot Aşama 1'den beri güvenli.
        core::diag::log(core::LogLevel::CRITICAL, rc, 0,
                        "app_core guvenlik zinciri kurulamadi");
    }

    for (;;)
    {
        domain::appcore::tick(core::Millis{millis()});
        runner.endCycle();
    }
}

} // namespace tasks
