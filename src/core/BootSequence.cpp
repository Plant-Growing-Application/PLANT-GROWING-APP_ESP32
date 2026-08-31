#include "BootSequence.h"

#include <Arduino.h>

#include "Diagnostics.h"

namespace core {
namespace boot {

const char* stageName(uint8_t stageId)
{
    switch (static_cast<BootStage>(stageId))
    {
    case BootStage::RESET_AND_WDT:   return "reset+wdt";
    case BootStage::GPIO_SAFE_STATE: return "gpio-safe";
    case BootStage::CORE_SERVICES:   return "core";
    case BootStage::CONFIG_LOAD:     return "config";
    case BootStage::FILESYSTEM:      return "filesystem";
    case BootStage::DISPLAY_HW:      return "display";
    case BootStage::SENSOR_HW:       return "sensor-hw";
    case BootStage::NETWORK_RADIO:   return "wifi-radio";
    case BootStage::TASK_CREATION:   return "tasks";
    }
    return "?";
}

SystemMode deriveMode(const BootReport& report)
{
    // Zorunlu aşama başarısız → sistem güvenli işleyemez.
    if (report.requiredStageFailed())
    {
        return SystemMode::SAFE;
    }
    // Zorunlu olmayan aşama başarısız → kalan işlevlerle devam.
    if (report.failedCount() > 0)
    {
        return SystemMode::DEGRADED;
    }
    return SystemMode::RUNNING;
}

SystemMode run(const BootStageDef* table, uint8_t count, BootReport& outReport)
{
    outReport.clear();

    if (table == nullptr || count == 0)
    {
        // Tablo yoksa çalıştırılacak aşama da yok. Boot yine DURMAZ;
        // sistem SAFE modda ayağa kalkar ve durumu raporlar.
        diag::log(LogLevel::CRITICAL, ErrCode::SYS_BOOT_STAGE_FAILED, 0,
                  "boot asama tablosu bos");
        return SystemMode::SAFE;
    }

    const uint32_t bootStart = millis();

    for (uint8_t i = 0; i < count; ++i)
    {
        const BootStageDef& def     = table[i];
        const uint8_t       stageId = static_cast<uint8_t>(def.stage);

        // Tablo hatası (null fonksiyon) sessizce geçilmez.
        if (def.fn == nullptr)
        {
            outReport.addStage(stageId, def.required, ErrCode::SYS_BOOT_STAGE_FAILED, 0);
            diag::log(LogLevel::ERROR, ErrCode::SYS_BOOT_STAGE_FAILED, stageId,
                      "asama fonksiyonu tanimsiz");
            continue;
        }

        const uint32_t t0     = millis();
        const ErrCode  result = def.fn();
        const uint32_t elapsedMs =
            static_cast<uint32_t>(millis() - t0);  // unsigned çıkarma: taşma güvenli

        const uint16_t durationMs =
            (elapsedMs > 0xFFFFu) ? 0xFFFFu : static_cast<uint16_t>(elapsedMs);

        if (!outReport.addStage(stageId, def.required, result, durationMs))
        {
            diag::log(LogLevel::WARNING, ErrCode::SYS_BOOT_STAGE_FAILED, stageId,
                      "boot raporu kapasitesi doldu");
        }

        // --- Sonucun raporlanması ---
        //
        // HİÇBİR başarısızlık boot'u durdurmaz (ARCHITECTURE P4). Aşama
        // başarısız olsa bile döngü devam eder; mevcut sistemdeki erken
        // `return` ve `while(true)` davranışlarının karşıtı.
        if (result != ErrCode::OK)
        {
            diag::log(def.required ? LogLevel::CRITICAL : LogLevel::ERROR, result, stageId,
                      def.required ? "ZORUNLU asama basarisiz" : "asama basarisiz");
        }

        // Yavaş aşama uyarısı: meşru bir init işleminin bu kadar sürmesi
        // beklenmez. Aşırı süre ya donanım beklemesine ya da aşama içinde
        // `delay()` kullanımına (yasak — CODING_STANDARDS Y3) işaret eder.
        if (elapsedMs > SLOW_STAGE_THRESHOLD.ms)
        {
            diag::log(LogLevel::WARNING, ErrCode::SYS_BOOT_STAGE_FAILED,
                      static_cast<int32_t>(elapsedMs), "asama beklenenden uzun surdu");
        }
    }

    const uint32_t totalMs = static_cast<uint32_t>(millis() - bootStart);
    outReport.totalDurationMs =
        (totalMs > 0xFFFFu) ? 0xFFFFu : static_cast<uint16_t>(totalMs);
    outReport.resetReason = diag::resetReason();

    const SystemMode mode = deriveMode(outReport);

    emitBootReport(outReport, &stageName);

    diag::log(mode == SystemMode::RUNNING ? LogLevel::INFO : LogLevel::WARNING,
              mode == SystemMode::RUNNING ? ErrCode::OK : ErrCode::SYS_BOOT_STAGE_FAILED,
              static_cast<int32_t>(mode), "boot tamamlandi");

    return mode;
}

} // namespace boot
} // namespace core
