#include "Diagnostics.h"

#include <Arduino.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdio.h>

namespace core {
namespace diag {
namespace {

// --- Korunan durum ---------------------------------------------------------
// Bu blok YALNIZCA `g_mutex` tutulurken okunur/yazılır.

SemaphoreHandle_t g_mutex = nullptr;

LogRecord g_ring[LOG_RING_CAPACITY];
uint8_t   g_ringHead  = 0;  ///< bir sonraki yazma konumu
uint8_t   g_ringCount = 0;  ///< tamponda geçerli kayıt sayısı
uint32_t  g_total     = 0;  ///< toplam yazılan kayıt (taşanlar dahil)

ErrCode  g_faults[MAX_ACTIVE_FAULTS];
uint8_t  g_faultCount    = 0;
uint16_t g_subsystemMask = 0;
bool     g_faultOverflow = false;

// --- Kilit gerektirmeyen durum ---------------------------------------------

BootReport  g_bootReport{};
ResetReason g_resetReason  = ResetReason::UNKNOWN;
LogLevel    g_serialLevel  = LogLevel::INFO;
volatile uint32_t g_droppedFromIsr = 0;

// --- Yardımcılar -----------------------------------------------------------

/// Alt sistem kimliğini bitmask konumuna çevirir.
/// Subsystem değerleri 0x01..0x0A olduğu için bit = (değer - 1).
inline uint16_t subsystemBit(Subsystem s)
{
    const uint8_t v = static_cast<uint8_t>(s);
    return (v >= 1 && v <= 16) ? static_cast<uint16_t>(1u << (v - 1)) : 0u;
}

/// Aktif hata listesinden alt sistem maskesini yeniden hesaplar.
/// Yalnızca clear() içinde çağrılır — raise() maskeyi artımlı günceller.
void rebuildSubsystemMask()
{
    uint16_t mask = 0;
    for (uint8_t i = 0; i < g_faultCount; ++i)
    {
        mask |= subsystemBit(subsystemOf(g_faults[i]));
    }
    g_subsystemMask = mask;
}

const char* levelName(LogLevel l)
{
    switch (l)
    {
    case LogLevel::INFO:     return "INFO";
    case LogLevel::WARNING:  return "WARN";
    case LogLevel::ERROR:    return "ERRO";
    case LogLevel::CRITICAL: return "CRIT";
    }
    return "????";
}

/// Kaydı seri porta yazar. KİLİT TUTULMADAN çağrılır (CODING_STANDARDS §7):
/// UART yazma yavaştır ve kritik bölgede yapılamaz.
///
/// Format makine tarafından ayrıştırılabilir; emoji kullanılmaz.
///   [    12345][WARN][0x0404] detail=3 mesaj
void emitSerial(const LogRecord& rec, const char* msg)
{
    if (static_cast<uint8_t>(rec.level) < static_cast<uint8_t>(g_serialLevel))
    {
        return;
    }

    char line[96];
    snprintf(line, sizeof(line), "[%10lu][%s][0x%04X] detail=%ld%s%s",
             static_cast<unsigned long>(rec.timestamp.v),
             levelName(rec.level),
             static_cast<unsigned>(rec.code),
             static_cast<long>(rec.detail),
             (msg != nullptr) ? " " : "",
             (msg != nullptr) ? msg : "");
    Serial.println(line);
}

/// Mutex'i sınırlı süreyle alır. Sonsuz bekleme yasaktır (CODING_STANDARDS §7):
/// tek bir hatalı task tüm sistemi kilitlememelidir.
inline bool lockAcquire()
{
    return g_mutex != nullptr && xSemaphoreTake(g_mutex, pdMS_TO_TICKS(20)) == pdTRUE;
}

inline void lockRelease()
{
    xSemaphoreGive(g_mutex);
}

} // namespace

// ---------------------------------------------------------------------------

ErrCode begin()
{
    if (g_mutex != nullptr)
    {
        return ErrCode::OK;
    }

    g_mutex = xSemaphoreCreateMutex();
    if (g_mutex == nullptr)
    {
        return ErrCode::SYS_BOOT_STAGE_FAILED;
    }

    g_bootReport.clear();
    return ErrCode::OK;
}

void log(LogLevel level, ErrCode code, int32_t detail, const char* msg)
{
    // ISR'den log çağrısı yasaktır (CODING_STANDARDS §6). Mutex almak ISR
    // bağlamında çökmeye yol açar; kaydı düşürüp sayacı artırıyoruz.
    // Bu sayacın sıfırdan farklı olması bir kural ihlaline işaret eder.
    if (xPortInIsrContext())
    {
        ++g_droppedFromIsr;
        return;
    }

    LogRecord rec;
    rec.timestamp = Millis{millis()};
    rec.code      = code;
    rec.level     = level;
    rec.reserved  = 0;
    rec.detail    = detail;

    if (lockAcquire())
    {
        g_ring[g_ringHead] = rec;
        g_ringHead         = static_cast<uint8_t>((g_ringHead + 1) % LOG_RING_CAPACITY);
        if (g_ringCount < LOG_RING_CAPACITY)
        {
            ++g_ringCount;
        }
        ++g_total;
        lockRelease();
    }

    // Seri port çıktısı kilit DIŞINDA — kritik bölge kısa kalmalı.
    emitSerial(rec, msg);
}

bool raise(ErrCode code, int32_t detail)
{
    if (code == ErrCode::OK || xPortInIsrContext())
    {
        return false;
    }

    bool isNew   = false;
    bool overflow = false;

    if (lockAcquire())
    {
        bool found = false;
        for (uint8_t i = 0; i < g_faultCount; ++i)
        {
            if (g_faults[i] == code)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            if (g_faultCount < MAX_ACTIVE_FAULTS)
            {
                g_faults[g_faultCount++] = code;
                g_subsystemMask |= subsystemBit(subsystemOf(code));
                isNew = true;
            }
            else
            {
                // Kapasite doldu: hata izlenemiyor. Sessiz kayıp olmasın diye
                // bayrak set edilir ve API'den okunabilir.
                g_faultOverflow = true;
                overflow        = true;
            }
        }
        lockRelease();
    }

    // Yalnızca YENİ aktifleşen hata loglanır. Her döngüde tekrarlanan bir hata
    // log selini tetiklemez (ARCHITECTURE §16 gözlemlenebilirlik dengesi).
    if (isNew)
    {
        log(LogLevel::ERROR, code, detail);
    }
    else if (overflow)
    {
        log(LogLevel::WARNING, ErrCode::SYS_LOW_HEAP, static_cast<int32_t>(code),
            "aktif hata kapasitesi doldu");
    }

    return isNew;
}

bool clear(ErrCode code)
{
    if (xPortInIsrContext())
    {
        return false;
    }

    bool removed = false;

    if (lockAcquire())
    {
        for (uint8_t i = 0; i < g_faultCount; ++i)
        {
            if (g_faults[i] == code)
            {
                g_faults[i] = g_faults[g_faultCount - 1];
                --g_faultCount;
                removed = true;
                break;
            }
        }
        if (removed)
        {
            rebuildSubsystemMask();
            if (g_faultCount < MAX_ACTIVE_FAULTS)
            {
                g_faultOverflow = false;
            }
        }
        lockRelease();
    }

    if (removed)
    {
        log(LogLevel::INFO, code, 0, "hata temizlendi");
    }
    return removed;
}

void activeFaults(FaultSummary& out)
{
    out.count         = 0;
    out.subsystemMask = 0;
    out.overflow      = false;

    if (lockAcquire())
    {
        out.count         = g_faultCount;
        out.subsystemMask = g_subsystemMask;
        out.overflow      = g_faultOverflow;
        for (uint8_t i = 0; i < g_faultCount; ++i)
        {
            out.faults[i] = g_faults[i];
        }
        lockRelease();
    }
}

uint8_t activeFaultCount()
{
    uint8_t n = 0;
    if (lockAcquire())
    {
        n = g_faultCount;
        lockRelease();
    }
    return n;
}

bool isActive(ErrCode code)
{
    bool found = false;
    if (lockAcquire())
    {
        for (uint8_t i = 0; i < g_faultCount; ++i)
        {
            if (g_faults[i] == code)
            {
                found = true;
                break;
            }
        }
        lockRelease();
    }
    return found;
}

uint8_t recent(LogRecord* out, uint8_t maxOut)
{
    if (out == nullptr || maxOut == 0)
    {
        return 0;
    }

    uint8_t n = 0;
    if (lockAcquire())
    {
        const uint8_t avail = (g_ringCount < maxOut) ? g_ringCount : maxOut;
        for (uint8_t i = 0; i < avail; ++i)
        {
            // En yeniden geriye doğru: head bir sonraki yazma konumudur.
            const uint8_t idx =
                static_cast<uint8_t>((g_ringHead + LOG_RING_CAPACITY - 1 - i) % LOG_RING_CAPACITY);
            out[i] = g_ring[idx];
        }
        n = avail;
        lockRelease();
    }
    return n;
}

uint32_t totalRecords()
{
    uint32_t n = 0;
    if (lockAcquire())
    {
        n = g_total;
        lockRelease();
    }
    return n;
}

uint32_t droppedFromIsr()
{
    return g_droppedFromIsr;
}

void setSerialLevel(LogLevel minLevel)
{
    g_serialLevel = minLevel;
}

ResetReason captureResetReason()
{
    switch (esp_reset_reason())
    {
    case ESP_RST_POWERON:  g_resetReason = ResetReason::POWER_ON;   break;
    case ESP_RST_SW:       g_resetReason = ResetReason::SOFTWARE;   break;
    case ESP_RST_PANIC:    g_resetReason = ResetReason::PANIC;      break;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      g_resetReason = ResetReason::WATCHDOG;   break;
    case ESP_RST_BROWNOUT: g_resetReason = ResetReason::BROWNOUT;   break;
    case ESP_RST_EXT:      g_resetReason = ResetReason::EXTERNAL_PIN; break;
    case ESP_RST_DEEPSLEEP: g_resetReason = ResetReason::DEEP_SLEEP; break;
    default:               g_resetReason = ResetReason::UNKNOWN;    break;
    }

    g_bootReport.resetReason = g_resetReason;

    // Watchdog ve panic kaynaklı reset tekrarlayan bir sorunun göstergesidir;
    // ARCHITECTURE §16.3 gereği CRITICAL olarak kaydedilir.
    if (g_resetReason == ResetReason::WATCHDOG)
    {
        log(LogLevel::CRITICAL, ErrCode::SYS_WATCHDOG_RESET,
            static_cast<int32_t>(g_resetReason), "onceki oturum WDT ile sonlandi");
    }
    else if (g_resetReason == ResetReason::PANIC)
    {
        log(LogLevel::CRITICAL, ErrCode::SYS_BOOT_STAGE_FAILED,
            static_cast<int32_t>(g_resetReason), "onceki oturum panic ile sonlandi");
    }

    return g_resetReason;
}

ResetReason resetReason()
{
    return g_resetReason;
}

BootReport& bootReport()
{
    return g_bootReport;
}

void reset()
{
    if (lockAcquire())
    {
        g_ringHead      = 0;
        g_ringCount     = 0;
        g_total         = 0;
        g_faultCount    = 0;
        g_subsystemMask = 0;
        g_faultOverflow = false;
        lockRelease();
    }
    g_bootReport.clear();
    g_droppedFromIsr = 0;
}

} // namespace diag
} // namespace core
