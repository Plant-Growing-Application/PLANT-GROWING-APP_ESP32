#include "WatchdogGuard.h"

#include <esp_err.h>
#include <esp_task_wdt.h>

#include <atomic>

#include "Diagnostics.h"

namespace core {
namespace wdt {
namespace {

std::atomic<uint8_t> g_subscribers{0};
Duration             g_timeout{0};
bool                 g_configured = false;

} // namespace

ErrCode begin(Duration timeout, bool panic)
{
    // TWDT saniye çözünürlüğünde çalışır. Aşağı yuvarlama koruma süresini
    // kısaltacağı için YUKARI yuvarlanır; en az 1 saniye.
    uint32_t seconds = (timeout.ms + 999u) / 1000u;
    if (seconds == 0u)
    {
        seconds = 1u;
    }

    // NOT: TWDT `setup()` çalışmadan önce IDF tarafından zaten başlatılmıştır.
    // Bu çağrı süreyi ve panic ayarını GÜNCELLER (esp_task_wdt.h: "If the TWDT
    // is already initialized ... this function will update the TWDT's timeout
    // period and panic configurations instead").
    const esp_err_t rc = esp_task_wdt_init(seconds, panic);
    if (rc != ESP_OK)
    {
        diag::log(LogLevel::CRITICAL, ErrCode::SYS_BOOT_STAGE_FAILED, static_cast<int32_t>(rc),
                  "TWDT yapilandirilamadi");
        return ErrCode::SYS_BOOT_STAGE_FAILED;
    }

    g_timeout    = Duration{seconds * 1000u};
    g_configured = true;

    diag::log(LogLevel::INFO, ErrCode::OK, static_cast<int32_t>(seconds),
              "TWDT yapilandirildi (saniye)");
    return ErrCode::OK;
}

ErrCode subscribe()
{
    // NULL = çağıran task'ın kendisi. Her task kendini kaydeder; başka bir
    // task adına kayıt yapılmaz (ARCHITECTURE §6.5).
    const esp_err_t rc = esp_task_wdt_add(nullptr);

    if (rc == ESP_OK)
    {
        g_subscribers.fetch_add(1, std::memory_order_relaxed);
        return ErrCode::OK;
    }

    if (rc == ESP_ERR_INVALID_ARG)
    {
        // Zaten kayıtlı — hata değil, tekrar çağrı.
        return ErrCode::OK;
    }

    // ESP_ERR_INVALID_STATE (TWDT hazır değil) veya ESP_ERR_NO_MEM.
    // Bir task'ın izlenemiyor olması sessizce geçilemez: mevcut projede
    // `Task_SensorLogger` hiç kaydolmamıştı ve kimse fark etmemişti.
    diag::log(LogLevel::CRITICAL, ErrCode::SYS_TASK_CREATE_FAILED, static_cast<int32_t>(rc),
              "TWDT kaydi basarisiz — task izlenmiyor");
    return ErrCode::SYS_TASK_CREATE_FAILED;
}

ErrCode unsubscribe()
{
    const esp_err_t rc = esp_task_wdt_delete(nullptr);

    if (rc == ESP_OK)
    {
        uint8_t prev = g_subscribers.load(std::memory_order_relaxed);
        while (prev > 0 &&
               !g_subscribers.compare_exchange_weak(prev, static_cast<uint8_t>(prev - 1),
                                                    std::memory_order_relaxed))
        {
            // compare_exchange_weak `prev`i günceller; döngü tekrar dener.
        }
        return ErrCode::OK;
    }

    if (rc == ESP_ERR_INVALID_ARG)
    {
        // Zaten kayıtsız — hata değil.
        return ErrCode::OK;
    }

    return ErrCode::SYS_TASK_CREATE_FAILED;
}

ErrCode feed()
{
    const esp_err_t rc = esp_task_wdt_reset();
    if (rc == ESP_OK)
    {
        return ErrCode::OK;
    }

    // ESP_ERR_NOT_FOUND: task kayıtlı değil. Bu bir programlama hatasıdır
    // (döngüde besleniyor ama kaydolmamış). Log YAPILMAZ: besleme her döngüde
    // çağrılır ve loglamak sel yaratır. Sessiz kalmaması için hata kodu döner
    // ve çağıran (TaskRunner, TASK-011) ilk kez gördüğünde raporlar.
    return ErrCode::SYS_TASK_CREATE_FAILED;
}

bool isSubscribed()
{
    return esp_task_wdt_status(nullptr) == ESP_OK;
}

uint8_t subscriberCount()
{
    return g_subscribers.load(std::memory_order_relaxed);
}

Duration timeout()
{
    return g_timeout;
}

bool isConfigured()
{
    return g_configured;
}

} // namespace wdt
} // namespace core
