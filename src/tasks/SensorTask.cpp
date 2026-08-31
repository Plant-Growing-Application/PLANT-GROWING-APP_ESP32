// io_sense task — TASK-027
//
// Döngü sırası `TaskRunner` tarafından yapısal olarak zorlanır:
//   iş → heartbeat → watchdog besleme → periyodik bekleme
//
// Bu task ADC1'in ve PCNT'nin TEK SAHİBİDİR (ARCHITECTURE §6.1, P2).

#include <Arduino.h>
#include "TaskConfig.h"
#include "TaskRunner.h"
#include "core/Diagnostics.h"
#include "services/SensorService.h"

namespace tasks {

void sensorTaskEntry(void*)
{
    TaskRunner runner(core::TaskId::IO_SENSE, core::TaskClass::SENSING,
                      core::millisecs(250));
    runner.begin();

    const core::ErrCode rc = services::sensorsvc::begin();
    if (rc != core::ErrCode::OK)
    {
        // Bir sensörün başlatılamaması sistemi durdurmaz: kalan sensörler
        // çalışır ve arızalı olan FAULT raporlar (ARCHITECTURE §16.3).
        core::diag::log(core::LogLevel::WARNING, rc, 0,
                        "bazi sensorler baslatilamadi — kalanlarla devam");
    }

    for (;;)
    {
        services::sensorsvc::tick(core::Millis{millis()});
        runner.endCycle();
    }
}

} // namespace tasks
