#include "StateStore.h"

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <string.h>

#include "Diagnostics.h"

namespace core {
namespace state {
namespace {

constexpr TickType_t LOCK_TIMEOUT_TICKS = pdMS_TO_TICKS(10);

SemaphoreHandle_t g_mutex = nullptr;
StaticSemaphore_t g_mutexStruct;

// --- Korunan durum ---------------------------------------------------------
SystemState g_state{};

// Tek yazar takibi: her bölümün ilk yazarı sahip olarak kaydedilir.
TaskHandle_t g_owner[static_cast<uint8_t>(StateSection::COUNT)] = {};

// --- İstatistikler (kilit altında güncellenir) ------------------------------
uint32_t g_publishCount   = 0;
uint32_t g_snapshotCount  = 0;
uint32_t g_lockTimeouts   = 0;
uint32_t g_ownViolations  = 0;
uint32_t g_maxLockHoldUs  = 0;

inline bool lockAcquire()
{
    return g_mutex != nullptr && xSemaphoreTake(g_mutex, LOCK_TIMEOUT_TICKS) == pdTRUE;
}

inline void lockRelease()
{
    xSemaphoreGive(g_mutex);
}

/// Bölümün sahibini doğrular. KİLİT İÇİNDE çağrılır.
///
/// İlk çağrıda sahibi kaydeder. Sonraki çağrılarda handle farklıysa ihlal
/// bildirir — ancak yazma İPTAL EDİLMEZ (bkz. tasarım kararı 2): programlama
/// hatasını çalışma zamanı davranış değişikliğine çevirmek ikinci bir hata
/// yaratır. İhlal görünür olmalı, sistemi bozmamalı.
bool checkOwnerLocked(StateSection section)
{
    const uint8_t      idx     = static_cast<uint8_t>(section);
    const TaskHandle_t current = xTaskGetCurrentTaskHandle();

    if (g_owner[idx] == nullptr)
    {
        g_owner[idx] = current;
        return true;
    }
    if (g_owner[idx] == current)
    {
        return true;
    }

    ++g_ownViolations;
    return false;
}

/// Ortak yayınlama gövdesi: kilidi al → kopyala → versiyonu artır → bırak.
///
/// Kritik bölge içinde log, seri port veya başka kilit YOKTUR
/// (CODING_STANDARDS §7). İhlal tespiti kilit içinde yapılır, loglama dışında.
template <typename T>
ErrCode publishSection(StateSection section, T& dest, const T& src)
{
    if (!lockAcquire())
    {
        ++g_lockTimeouts;
        return ErrCode::SYS_BOOT_STAGE_FAILED;
    }

    const int64_t enterUs = esp_timer_get_time();

    const bool ownerOk = checkOwnerLocked(section);
    dest               = src;
    ++g_state.version;
    ++g_publishCount;

    const uint32_t heldUs = static_cast<uint32_t>(esp_timer_get_time() - enterUs);
    if (heldUs > g_maxLockHoldUs)
    {
        g_maxLockHoldUs = heldUs;
    }

    lockRelease();

    // Loglama KİLİT DIŞINDA — Diagnostics kendi mutex'ini alır.
    if (!ownerOk)
    {
        diag::log(LogLevel::CRITICAL, ErrCode::SYS_TASK_HEARTBEAT_LOST,
                  static_cast<int32_t>(section), "tek yazar kurali ihlali");
    }

    return ErrCode::OK;
}

} // namespace

ErrCode begin()
{
    if (g_mutex != nullptr)
    {
        return ErrCode::OK;
    }

    // Statik ayırma: heap kullanılmaz (CODING_STANDARDS §5).
    g_mutex = xSemaphoreCreateMutexStatic(&g_mutexStruct);
    if (g_mutex == nullptr)
    {
        return ErrCode::SYS_BOOT_STAGE_FAILED;
    }

    memset(&g_state, 0, sizeof(g_state));
    for (uint8_t i = 0; i < static_cast<uint8_t>(StateSection::COUNT); ++i)
    {
        g_owner[i] = nullptr;
    }
    return ErrCode::OK;
}

ErrCode publishSystem(const SystemStatus& v)
{
    return publishSection(StateSection::SYSTEM, g_state.system, v);
}

ErrCode publishNetwork(const NetworkStatus& v)
{
    return publishSection(StateSection::NETWORK, g_state.network, v);
}

ErrCode publishSensors(const SensorsStatus& v)
{
    return publishSection(StateSection::SENSORS, g_state.sensors, v);
}

ErrCode publishActuators(const ActuatorsStatus& v)
{
    return publishSection(StateSection::ACTUATORS, g_state.actuators, v);
}

ErrCode publishSafety(const SafetyStatus& v)
{
    return publishSection(StateSection::SAFETY, g_state.safety, v);
}

ErrCode publishAutomation(const AutomationStatus& v)
{
    return publishSection(StateSection::AUTOMATION, g_state.automation, v);
}

ErrCode publishTime(const TimeStatus& v)
{
    return publishSection(StateSection::TIME, g_state.time, v);
}

ErrCode snapshot(SystemState& out)
{
    if (!lockAcquire())
    {
        // `out` DEĞİŞTİRİLMEZ: çağıran bir önceki kopyasıyla devam edebilir.
        ++g_lockTimeouts;
        return ErrCode::SYS_BOOT_STAGE_FAILED;
    }

    const int64_t enterUs = esp_timer_get_time();

    out = g_state;
    ++g_snapshotCount;

    const uint32_t heldUs = static_cast<uint32_t>(esp_timer_get_time() - enterUs);
    if (heldUs > g_maxLockHoldUs)
    {
        g_maxLockHoldUs = heldUs;
    }

    lockRelease();
    return ErrCode::OK;
}

uint32_t version()
{
    uint32_t v = 0;
    if (lockAcquire())
    {
        v = g_state.version;
        lockRelease();
    }
    else
    {
        ++g_lockTimeouts;
    }
    return v;
}

void stats(StateStoreStats& out)
{
    if (lockAcquire())
    {
        out.publishCount        = g_publishCount;
        out.snapshotCount       = g_snapshotCount;
        out.lockTimeouts        = g_lockTimeouts;
        out.ownershipViolations = g_ownViolations;
        out.maxLockHoldUs       = g_maxLockHoldUs;
        lockRelease();
    }
    else
    {
        memset(&out, 0, sizeof(out));
    }
}

void clearOwner(StateSection section)
{
    if (section >= StateSection::COUNT)
    {
        return;
    }
    if (lockAcquire())
    {
        g_owner[static_cast<uint8_t>(section)] = nullptr;
        lockRelease();
    }
}

void reset()
{
    if (lockAcquire())
    {
        memset(&g_state, 0, sizeof(g_state));
        for (uint8_t i = 0; i < static_cast<uint8_t>(StateSection::COUNT); ++i)
        {
            g_owner[i] = nullptr;
        }
        g_publishCount  = 0;
        g_snapshotCount = 0;
        g_lockTimeouts  = 0;
        g_ownViolations = 0;
        g_maxLockHoldUs = 0;
        lockRelease();
    }
}

} // namespace state
} // namespace core
