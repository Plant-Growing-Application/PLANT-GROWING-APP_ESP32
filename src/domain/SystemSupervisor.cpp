#include "SystemSupervisor.h"

#include <Arduino.h>
#include <esp_attr.h>
#include <esp_system.h>

#include "core/Diagnostics.h"
#include "core/StateStore.h"
#include "core/WatchdogGuard.h"

namespace domain {
namespace supervisor {
namespace {

using core::ErrCode;
using core::SystemMode;

// --- RTC belleğinde kalıcı yeniden başlatma kaydı ---------------------------
//
// `RTC_NOINIT_ATTR` yazılım reset'inde SIFIRLANMAZ. Güç kesintisinde kaybolur;
// o durumda reset nedeni zaten POWER_ON okunacağı için bilgi kaybı yoktur.
//
// Bu, NVS'e (services/ katmanı) bağımlı olmadan "yeniden başlatma nedeni
// kalıcı kaydedilsin" gereksinimini karşılar — domain/ → services/ bağımlılığı
// yasaktır (D5).
constexpr uint32_t RESTART_MAGIC = 0x52535431u;  // "RST1"

struct RestartRecord
{
    uint32_t magic;
    uint16_t reason;
    uint16_t checksum;
};

RTC_NOINIT_ATTR RestartRecord g_restartRecord;

constexpr uint16_t restartChecksum(uint16_t reason)
{
    return static_cast<uint16_t>(reason ^ 0xA5A5u);
}

// --- Modül durumu (yalnızca app_core task'ından erişilir) --------------------

SystemMode       g_mode            = SystemMode::BOOTING;
uint16_t         g_degradedMask    = DEGRADED_NONE;
SafeStateHandler g_safeStateFn     = nullptr;
ErrCode          g_prevRestart     = ErrCode::OK;

bool             g_restartPending  = false;
core::Millis     g_restartRequestedAt{0};
core::Duration   g_restartDelay{0};
ErrCode          g_restartReason   = ErrCode::OK;

uint32_t         g_minFreeHeap     = 0xFFFFFFFFu;

/// Bir task'ın heartbeat'i sınıfına göre belirlenen eşiği aştı mı?
bool isTaskStalled(core::TaskId id, core::Millis now)
{
    core::TaskHealth h;
    core::taskreg::health(id, h);

    // Kaydolmamış task izlenmez: henüz başlamamış olabilir.
    if (h.registered == 0)
    {
        return false;
    }

    const core::Duration since = core::taskreg::sinceLastBeat(id, now);
    return since.ms > core::softDeadline(h.taskClass).ms;
}

void publishSystemState(core::Millis now)
{
    core::FaultSummary faults;
    core::diag::activeFaults(faults);

    const uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < g_minFreeHeap)
    {
        g_minFreeHeap = freeHeap;
    }

    // §16.3 "Heap kritik seviyede": tespit BURADA, kısıtlama tüketicilerde.
    // Tüketiciler eşiği snapshot'taki `freeHeapBytes` üzerinden okur —
    // supervisor'a bağımlılık kurmadan (katman temizliği).
    if (freeHeap < core::LOW_HEAP_BYTES)
    {
        core::diag::raise(core::ErrCode::SYS_LOW_HEAP, static_cast<int32_t>(freeHeap));
    }
    else
    {
        core::diag::clear(core::ErrCode::SYS_LOW_HEAP);
    }

    core::SystemStatus s{};
    s.uptimeMs           = now.v;
    s.freeHeapBytes      = freeHeap;
    s.minFreeHeapBytes   = g_minFreeHeap;
    s.faultSubsystemMask = faults.subsystemMask;
    s.mode               = g_mode;
    s.activeFaultCount   = faults.count;
    s.bootFailedStages   = core::diag::bootReport().failedCount();
    s.resetReason        = static_cast<uint8_t>(core::diag::resetReason());

    core::state::publishSystem(s);
}

/// Aktüatörleri güvenli duruma alır. İşleyici kayıtlı değilse bu bir
/// yapılandırma hatasıdır ve sessizce geçilemez.
void enterSafeOutputs(ErrCode reason)
{
    if (g_safeStateFn != nullptr)
    {
        g_safeStateFn(reason);
        return;
    }
    core::diag::log(core::LogLevel::CRITICAL, ErrCode::SYS_BOOT_STAGE_FAILED,
                    static_cast<int32_t>(reason),
                    "guvenli durum isleyicisi kayitli degil");
}

} // namespace

// ---------------------------------------------------------------------------

bool canTransition(SystemMode from, SystemMode to)
{
    if (from == to)
    {
        return true;
    }

    // EMERGENCY her yerden, koşulsuz girilebilir: güvenlik yolu hiçbir engele
    // takılmamalıdır.
    if (to == SystemMode::EMERGENCY)
    {
        return true;
    }

    switch (from)
    {
    case SystemMode::BOOTING:
        // Boot sonucu: RUNNING / DEGRADED / SAFE
        return to == SystemMode::RUNNING || to == SystemMode::DEGRADED ||
               to == SystemMode::SAFE;

    case SystemMode::RUNNING:
        return to == SystemMode::DEGRADED || to == SystemMode::SAFE;

    case SystemMode::DEGRADED:
        return to == SystemMode::RUNNING || to == SystemMode::SAFE;

    case SystemMode::SAFE:
        // Neden ortadan kalkınca normale dönülebilir.
        return to == SystemMode::RUNNING || to == SystemMode::DEGRADED;

    case SystemMode::EMERGENCY:
        // Mandallı: yalnızca açık operatör onayıyla çıkılır (TASK-032).
        // BOOTING'e dönüş yok.
        return to == SystemMode::RUNNING || to == SystemMode::DEGRADED;
    }
    return false;
}

bool setMode(SystemMode target, ErrCode reason)
{
    if (g_mode == target)
    {
        return true;
    }

    if (!canTransition(g_mode, target))
    {
        // REDDEDİLİR. Geçersiz geçişi uygulamak sistemi tutarsız bir moda
        // sokar; bilinmeyen bir moda geçmektense bilinen modda kalmak yeğdir.
        core::diag::log(core::LogLevel::CRITICAL, ErrCode::SYS_BOOT_STAGE_FAILED,
                        static_cast<int32_t>(static_cast<uint8_t>(g_mode) << 8 |
                                             static_cast<uint8_t>(target)),
                        "izinsiz mod gecisi reddedildi");
        return false;
    }

    const SystemMode previous = g_mode;
    g_mode                    = target;

    core::diag::log(target == SystemMode::RUNNING ? core::LogLevel::INFO
                                                  : core::LogLevel::WARNING,
                    reason,
                    static_cast<int32_t>(static_cast<uint8_t>(previous) << 8 |
                                         static_cast<uint8_t>(target)),
                    "mod degisti");
    return true;
}

void begin()
{
    // Önceki oturumda kontrollü yeniden başlatma yapılmış mı?
    if (g_restartRecord.magic == RESTART_MAGIC &&
        g_restartRecord.checksum == restartChecksum(g_restartRecord.reason))
    {
        g_prevRestart = static_cast<ErrCode>(g_restartRecord.reason);
        core::diag::log(core::LogLevel::INFO, g_prevRestart, 0,
                        "onceki oturum kontrollu yeniden baslatma ile sonlandi");
    }
    else
    {
        g_prevRestart = ErrCode::OK;
    }

    // Kayıt tüketildi: bir sonraki boot'ta tekrar raporlanmasın.
    g_restartRecord.magic = 0;

    g_mode           = SystemMode::BOOTING;
    g_degradedMask   = DEGRADED_NONE;
    g_restartPending = false;
    g_minFreeHeap    = ESP.getFreeHeap();
}

void setSafeStateHandler(SafeStateHandler handler)
{
    g_safeStateFn = handler;
}

void applyBootResult(SystemMode bootMode, uint8_t failedStageCount)
{
    if (failedStageCount > 0)
    {
        // Boot kaynaklı DEGRADED nedeni kendiliğinden temizlenmez:
        // boot'ta mount edilemeyen bir dosya sistemi çalışma zamanında
        // kendini düzeltmez.
        g_degradedMask |= DEGRADED_BOOT_STAGE;
    }

    setMode(bootMode, bootMode == SystemMode::RUNNING ? ErrCode::OK
                                                      : ErrCode::SYS_BOOT_STAGE_FAILED);
}

void tick(core::Millis now)
{
    // --- 1) Heartbeat izleme -------------------------------------------------
    //
    // NOT: `app_core` KENDİNİ izleyemez — bu kod onun içinde çalışır. Bu boşluk
    // kapatılamaz; app_core için tek koruma donanım watchdog'udur (8 sn).
    bool anyStalled = false;
    core::TaskId stalledId = core::TaskId::COUNT;

    for (uint8_t i = 0; i < static_cast<uint8_t>(core::TaskId::COUNT); ++i)
    {
        const core::TaskId id = static_cast<core::TaskId>(i);
        if (id == core::TaskId::APP_CORE)
        {
            continue;  // kendini izleyemez
        }
        if (isTaskStalled(id, now))
        {
            anyStalled = true;
            stalledId  = id;
            break;
        }
    }

    if (anyStalled)
    {
        if ((g_degradedMask & DEGRADED_TASK_STALL) == 0)
        {
            g_degradedMask |= DEGRADED_TASK_STALL;

            // SIRA ÖNEMLİ: ÖNCE aktüatörler güvenli duruma, SONRA mod değişimi.
            enterSafeOutputs(ErrCode::SYS_TASK_HEARTBEAT_LOST);

            core::diag::log(core::LogLevel::CRITICAL, ErrCode::SYS_TASK_HEARTBEAT_LOST,
                            static_cast<int32_t>(stalledId), "task heartbeat bayatladi");
            setMode(SystemMode::DEGRADED, ErrCode::SYS_TASK_HEARTBEAT_LOST);
        }
    }
    else if ((g_degradedMask & DEGRADED_TASK_STALL) != 0)
    {
        g_degradedMask &= static_cast<uint16_t>(~DEGRADED_TASK_STALL);
        core::diag::log(core::LogLevel::INFO, ErrCode::OK, 0, "task heartbeat normale dondu");
    }

    // --- 2) Aktif hata nedeni -----------------------------------------------
    if (core::diag::activeFaultCount() > 0)
    {
        g_degradedMask |= DEGRADED_ACTIVE_FAULT;
    }
    else
    {
        g_degradedMask &= static_cast<uint16_t>(~DEGRADED_ACTIVE_FAULT);
    }

    // --- 3) DEGRADED ↔ RUNNING geçişi ---------------------------------------
    //
    // Dönüş koşulu AÇIK: neden maskesi sıfırlanınca RUNNING'e dönülür.
    // Belirsiz bırakılsaydı sistem kalıcı olarak DEGRADED'da takılırdı.
    if (g_mode == SystemMode::RUNNING && g_degradedMask != DEGRADED_NONE)
    {
        setMode(SystemMode::DEGRADED, ErrCode::SYS_BOOT_STAGE_FAILED);
    }
    else if (g_mode == SystemMode::DEGRADED && g_degradedMask == DEGRADED_NONE)
    {
        setMode(SystemMode::RUNNING, ErrCode::OK);
    }

    // --- 4) Bekleyen yeniden başlatma ---------------------------------------
    // Taşma güvenli desen: talep ANINI sakla, GEÇEN SÜREYİ süreyle karşılaştır.
    // (Gelecekteki bir damgayı `elapsed()` ile karşılaştırmak unsigned sarma
    //  nedeniyle koşulu HEMEN doğru yapar — gecikme hiç çalışmazdı.)
    if (g_restartPending && core::hasElapsed(now, g_restartRequestedAt, g_restartDelay))
    {
        enterSafeOutputs(g_restartReason);

        g_restartRecord.reason   = static_cast<uint16_t>(g_restartReason);
        g_restartRecord.checksum = restartChecksum(g_restartRecord.reason);
        g_restartRecord.magic    = RESTART_MAGIC;

        core::diag::log(core::LogLevel::CRITICAL, g_restartReason, 0,
                        "kontrollu yeniden baslatma");
        Serial.flush();
        esp_restart();
    }

    // --- 5) State yayınla ----------------------------------------------------
    publishSystemState(now);
}

SystemMode mode()
{
    return g_mode;
}

uint16_t degradedReasons()
{
    return g_degradedMask;
}

void requestRestart(ErrCode reason, core::Duration delay)
{
    if (g_restartPending)
    {
        return;  // ilk talep geçerli
    }
    g_restartPending = true;
    g_restartReason  = reason;
    g_restartRequestedAt = core::Millis{millis()};
    g_restartDelay       = delay;

    core::diag::log(core::LogLevel::WARNING, reason, static_cast<int32_t>(delay.ms),
                    "yeniden baslatma planlandi (ms)");
}

bool restartPending()
{
    return g_restartPending;
}

ErrCode previousRestartReason()
{
    return g_prevRestart;
}

} // namespace supervisor
} // namespace domain
