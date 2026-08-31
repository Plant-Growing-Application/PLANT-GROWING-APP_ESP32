#include "BootReport.h"

#include <Arduino.h>
#include <stdio.h>

namespace core {
namespace {

const char* resetReasonName(ResetReason r)
{
    switch (r)
    {
    case ResetReason::POWER_ON:     return "POWER_ON";
    case ResetReason::SOFTWARE:     return "SOFTWARE";
    case ResetReason::PANIC:        return "PANIC";
    case ResetReason::WATCHDOG:     return "WATCHDOG";
    case ResetReason::BROWNOUT:     return "BROWNOUT";
    case ResetReason::EXTERNAL_PIN: return "EXT_PIN";
    case ResetReason::DEEP_SLEEP:   return "DEEP_SLEEP";
    case ResetReason::UNKNOWN:      break;
    }
    return "UNKNOWN";
}

} // namespace

void emitBootReport(const BootReport& report, StageNameFn nameFn)
{
    char line[96];

    snprintf(line, sizeof(line), "[BOOT] reset=%s stages=%u failed=%u total=%ums",
             resetReasonName(report.resetReason),
             static_cast<unsigned>(report.stageCount),
             static_cast<unsigned>(report.failedCount()),
             static_cast<unsigned>(report.totalDurationMs));
    Serial.println(line);

    for (uint8_t i = 0; i < report.stageCount; ++i)
    {
        const BootStageResult& s = report.stages[i];

        // Zorunlu bir aşamanın başarısızlığı SAFE moda götürür; çıktıda
        // ayırt edilebilir olmalı.
        const char* mark = (s.result == ErrCode::OK) ? "ok  "
                           : (s.required != 0u)      ? "FAIL"
                                                     : "warn";

        snprintf(line, sizeof(line), "[BOOT] %-2u %-16s %s 0x%04X %ums",
                 static_cast<unsigned>(s.stageId),
                 (nameFn != nullptr) ? nameFn(s.stageId) : "?",
                 mark,
                 static_cast<unsigned>(s.result),
                 static_cast<unsigned>(s.durationMs));
        Serial.println(line);
    }
}

} // namespace core
