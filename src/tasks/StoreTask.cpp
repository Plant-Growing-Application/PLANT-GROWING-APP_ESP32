// store task — TASK-059
//
// EN DÜŞÜK ÖNCELİK (1). Flash yazması gerçek zamanlı bir iş değildir ve
// güvenlik döngüsünü hiçbir koşulda geciktirmemelidir.
//
// Eski projede en yüksek öncelik LOGLAMA task'ındaydı; güvenlik kararları
// loglamanın arkasında bekliyordu. Bu ters çevrildi (ARCHITECTURE §6.3).
//
// OLAY GÜDÜMLÜ: `storage::tick()` kuyruk boşken bloklar (CPU harcamaz) ama
// en fazla 1 sn sonra döner ki heartbeat beslenebilsin. Bu yüzden burada
// `TaskRunner`'ın periyodik beklemesi 0 ms — bekleme kuyruğun içinde.

#include <Arduino.h>

#include "TaskConfig.h"
#include "TaskRunner.h"
#include "core/Diagnostics.h"
#include "services/StorageService.h"

namespace tasks {

void storeTaskEntry(void*)
{
    // Periyot 0: gerçek bekleme `storage::tick()` içindeki kuyruk
    // beklemesidir. `vTaskDelayUntil` ile ikinci bir bekleme eklemek
    // yazma gecikmesini gereksiz yere artırırdı.
    TaskRunner runner(core::TaskId::STORE, core::TaskClass::STORAGE, core::millisecs(0));
    runner.begin();

    const core::ErrCode rc = services::storage::begin();
    if (rc != core::ErrCode::OK)
    {
        // Depolama kurulamadıysa sistem DURMAZ: sensörler okunur, güvenlik
        // çalışır, yalnızca geçmiş ve kalıcılık kaybolur (P4).
        core::diag::log(core::LogLevel::ERROR, rc, 0, "depolama servisi baslatilamadi");
    }

    for (;;)
    {
        services::storage::tick(core::Millis{millis()});
        runner.endCycle();
    }
}

} // namespace tasks
