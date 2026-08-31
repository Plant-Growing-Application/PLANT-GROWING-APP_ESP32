// Topraksız Tarım Sistemi — giriş noktası
//
// Bu dosya BİLİNÇLİ OLARAK İNCEDİR. Boot sırası `BootWiring.cpp`'de,
// karar mantığı `core/BootSequence.cpp`'de, iş `tasks/` altındadır.
//
// ── `loop()` KULLANILMIYOR ──────────────────────────────────────────────────
// Tüm iş FreeRTOS task'larında yapılır. `loopTaskWDTEnabled = false`
// (TASK-010): `loop()` watchdog beslemediği için izlenmemelidir.
//
// Eski projede `loop()` içinde sensör okuma, WebSocket temizliği ve ekran
// çizimi vardı; hepsi artık kendi task'ında.

#include <Arduino.h>

#include "BootWiring.h"
#include "core/BootReport.h"
#include "core/Diagnostics.h"
#include "domain/SystemSupervisor.h"

namespace {
core::BootReport g_bootReport{};
}

void setup()
{
    // Seri port önce: Aşama 0'ın kendisi bile bir şey söylemek isteyebilir.
    // `begin()` bloklamaz.
    Serial.begin(115200);

    // Boot aşamaları. Aşama 1 (röleler güvenli) buranın hemen içinde,
    // her şeyden önce çalışır (ARCHITECTURE §7.1).
    const core::SystemMode mode = app::runBoot(g_bootReport);

    // Supervisor'a boot sonucunu bildir: mod ondan türetilir ve
    // DEGRADED nedeni kalıcı olarak kaydedilir (TASK-012).
    domain::supervisor::begin();
    domain::supervisor::applyBootResult(mode, g_bootReport.failedCount());
}

void loop()
{
    // Arduino loop'u kullanılmıyor. Boş bir döngü CPU harcadığı için
    // pasif bekleniyor; `loopTaskWDTEnabled = false` olduğu için watchdog
    // bu task'ı izlemiyor.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
