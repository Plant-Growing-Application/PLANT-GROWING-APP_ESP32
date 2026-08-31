#pragma once

// Boot raporu deposu — TASK-005
//
// Bu dosya yalnızca VERİ YAPISINI tanımlar. Aşama kimlikleri, aşama yürütücüsü
// ve boot akışı TASK-010 kapsamındadır (ARCHITECTURE §7.1).
//
// Neden gerekli: mevcut sistemde bir init hatası yalnızca seri porta yazılıyordu.
// Sahada seri port yoktur; kullanıcı neyin çalışmadığını göremiyordu. Boot raporu
// RAM'de kalıcıdır ve web/OLED'den okunabilir (TASK-043, TASK-052).

#include <stdint.h>
#include <type_traits>

#include "ErrorCodes.h"
#include "Time.h"

namespace core {

/// Boot raporunda saklanabilecek en fazla aşama sayısı.
/// ARCHITECTURE §7.1'de 10 aşama tanımlı; büyüme payı bırakıldı.
constexpr uint8_t MAX_BOOT_STAGES = 12;

/// Cihazın neden yeniden başladığı. ESP-IDF tipinden bağımsız tutulur ki
/// `core/` katmanının arayüzü platforma bağlanmasın (D5).
enum class ResetReason : uint8_t
{
    UNKNOWN     = 0,
    POWER_ON    = 1,  ///< normal güç verme
    SOFTWARE    = 2,  ///< kontrollü yeniden başlatma (TASK-012)
    PANIC       = 3,  ///< çökme / exception
    WATCHDOG    = 4,  ///< task veya interrupt watchdog — CRITICAL (§16.3)
    BROWNOUT    = 5,  ///< besleme gerilimi düştü
    EXTERNAL_PIN = 6, ///< reset pini (isim `EXTERNAL` degil: Arduino.h `#define EXTERNAL 0`)
    DEEP_SLEEP  = 7,
};

/// Tek bir boot aşamasının sonucu.
struct BootStageResult
{
    uint8_t  stageId;     ///< aşama kimliği — anlamını TASK-010 tanımlar
    uint8_t  required;    ///< 1 = zorunlu aşama (başarısızlığı SAFE moda götürür)
    ErrCode  result;      ///< ErrCode::OK veya aşamanın döndürdüğü hata
    uint16_t durationMs;  ///< aşama süresi (65 sn üzeri beklenmiyor)
};

/// Açılış sürecinin tam raporu.
struct BootReport
{
    ResetReason     resetReason;
    uint8_t         stageCount;
    uint16_t        totalDurationMs;
    BootStageResult stages[MAX_BOOT_STAGES];

    void clear()
    {
        resetReason     = ResetReason::UNKNOWN;
        stageCount      = 0;
        totalDurationMs = 0;
    }

    /// Aşama sonucu ekler. Kapasite dolduysa `false` döner — sessiz kayıp olmaz.
    bool addStage(uint8_t stageId, bool isRequired, ErrCode result, uint16_t durationMs)
    {
        if (stageCount >= MAX_BOOT_STAGES)
        {
            return false;
        }
        stages[stageCount].stageId    = stageId;
        stages[stageCount].required   = isRequired ? 1u : 0u;
        stages[stageCount].result     = result;
        stages[stageCount].durationMs = durationMs;
        ++stageCount;
        return true;
    }

    /// Başarısız aşama sayısı.
    uint8_t failedCount() const
    {
        uint8_t n = 0;
        for (uint8_t i = 0; i < stageCount; ++i)
        {
            if (stages[i].result != ErrCode::OK)
            {
                ++n;
            }
        }
        return n;
    }

    /// Zorunlu bir aşama başarısız oldu mu? (TASK-012 mod kararı için.)
    bool requiredStageFailed() const
    {
        for (uint8_t i = 0; i < stageCount; ++i)
        {
            if (stages[i].required != 0u && stages[i].result != ErrCode::OK)
            {
                return true;
            }
        }
        return false;
    }
};

static_assert(std::is_trivially_copyable<BootReport>::value,
              "BootReport trivially copyable olmali");
static_assert(sizeof(BootStageResult) <= 8, "BootStageResult 8 bayti asmamali");
static_assert(sizeof(BootReport) <= 128, "BootReport 128 bayti asmamali");

// --- Rapor çıktısı — TASK-010 ----------------------------------------------
//
// Uygulaması `BootReport.cpp` içindedir (seri port erişimi gerektirir).
//
// Aşama ADLARI `BootSequence`'ta tanımlıdır. Döngüsel bağımlılığı önlemek için
// ad çözümleme dışarıdan bir fonksiyon işaretçisiyle verilir; bu dosya
// `BootSequence.h`'ı include etmez.

/// Aşama kimliğini insan okunabilir ada çeviren fonksiyon.
using StageNameFn = const char* (*)(uint8_t);

/// Raporu seri porta, makine tarafından ayrıştırılabilir biçimde yazar.
///
/// Sahada seri port YOKTUR; bu çıktı yalnızca geliştirme içindir. Raporun
/// kalıcı hâli RAM'dedir ve web/OLED üzerinden okunur (TASK-043, TASK-052) —
/// mevcut sistemdeki "yalnızca Serial.println" yaklaşımının aksine.
void emitBootReport(const BootReport& report, StageNameFn nameFn);

} // namespace core
