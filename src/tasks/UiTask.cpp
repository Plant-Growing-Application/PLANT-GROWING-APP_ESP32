// ui task — TASK-053
//
// İNCE SARMALAYICI: tüm arayüz mantığı `interfaces/ui/UiService` içinde ve
// `now` parametreli — host'ta test edilebilir.
//
// Bu task OLED'in ve girdi aygıtlarının TEK SAHİBİDİR (ARCHITECTURE §6.1).
// Eski projede OLED'e hem `Task_Display` hem `Sensor::SensorValues()`
// yazıyordu; iki task aynı I2C aygıtını sürdüğü için ekran bozuluyordu.
//
// Wi-Fi LED'i için AYRI TASK YOK: eski `Task_WifiLed` 2 KB stack harcıyordu
// (§6.4).

#include <Arduino.h>

#include "TaskConfig.h"
#include "TaskRunner.h"
#include "core/Diagnostics.h"
#include "hal/InputDevices.h"
#include "interfaces/ui/UiService.h"

namespace tasks {

void uiTaskEntry(void*)
{
    TaskRunner runner(core::TaskId::UI, core::TaskClass::UI, core::millisecs(50));
    runner.begin();

    const core::ErrCode rc = interfaces::ui::begin();
    if (rc != core::ErrCode::OK)
    {
        core::diag::log(core::LogLevel::WARNING, rc, 0, "arayuz baslatilamadi");
    }

    for (;;)
    {
        interfaces::ui::tick(core::Millis{millis()});
        runner.endCycle();
    }
}

} // namespace tasks
