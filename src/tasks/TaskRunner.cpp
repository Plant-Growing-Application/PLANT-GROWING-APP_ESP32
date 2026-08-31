#include "TaskRunner.h"

#include <Arduino.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "TaskConfig.h"
#include "core/Diagnostics.h"

namespace tasks {
namespace {

/// Stack watermark ölçümü stack'i tarar; maliyeti stack boyutuyla orantılıdır.
/// `ui` task'ı 20 Hz çalışıyor — her döngüde ölçmek israftır. Watermark yavaş
/// değişen bir büyüklük olduğu için seyrek örneklemek yeterlidir.
constexpr uint32_t STACK_SAMPLE_INTERVAL_CYCLES = 64;

} // namespace

TaskRunner::TaskRunner(core::TaskId id, core::TaskClass cls, core::Duration period)
    : _id(id)
    , _class(cls)
    , _period(period)
    , _lastWakeTicks(0)
    , _cycleStartUs(0)
    , _cycles(0)
    , _overruns(0)
    , _feedFailedLogged(false)
{
}

void TaskRunner::begin()
{
    // Her task KENDİNİ kaydeder; başka bir task adına kayıt yapılmaz.
    // Kayıt, task'ın gerçekten çalışmaya başladığını kanıtlar.
    const core::ErrCode rc = core::wdt::subscribe();
    if (rc != core::ErrCode::OK)
    {
        core::diag::log(core::LogLevel::CRITICAL, rc, static_cast<int32_t>(_id),
                        "task TWDT'ye kaydolamadi — izlenmiyor");
    }

    core::taskreg::registerSelf(_id, _class);

    _lastWakeTicks = xTaskGetTickCount();
    _cycleStartUs  = static_cast<uint32_t>(esp_timer_get_time());
}

void TaskRunner::endCycle()
{
    const uint32_t nowUs = static_cast<uint32_t>(esp_timer_get_time());
    const uint32_t loopUs = nowUs - _cycleStartUs;  // unsigned: taşma güvenli

    const bool overran = loopUs > (_period.ms * 1000u);
    if (overran)
    {
        ++_overruns;
    }

    // --- 1) Heartbeat ---
    core::taskreg::beat(_id, loopUs, overran);

    // Stack watermark: seyrek örneklenir (bkz. STACK_SAMPLE_INTERVAL_CYCLES).
    // ESP-IDF bu değeri BAYT olarak döndürür (vanilla FreeRTOS'tan farklı).
    if ((_cycles % STACK_SAMPLE_INTERVAL_CYCLES) == 0u)
    {
        const UBaseType_t freeBytes = uxTaskGetStackHighWaterMark(nullptr);
        core::taskreg::updateStack(
            _id, (freeBytes > 0xFFFFu) ? 0xFFFFu : static_cast<uint16_t>(freeBytes));
    }
    ++_cycles;

    // --- 2) Watchdog besleme — DÖNGÜNÜN EN SONU ---
    //
    // Tüm iş bittikten sonra beslenir. Ortada beslemek task'ın ilerlediğini
    // kanıtlamaz ve watchdog'u anlamsız kılar.
    const core::ErrCode feedRc = core::wdt::feed();
    if (feedRc != core::ErrCode::OK && !_feedFailedLogged)
    {
        // `feed()` her döngüde çağrılır; loglamak sel yaratır. Bu yüzden
        // yalnızca İLK başarısızlık raporlanır — sessiz de kalmaz.
        _feedFailedLogged = true;
        core::diag::log(core::LogLevel::ERROR, feedRc, static_cast<int32_t>(_id),
                        "watchdog beslenemedi — task kayitli degil");
    }

    // --- 3) Periyodik bekleme ---
    //
    // Mutlak uyanma zamanı: iş süresi periyoda EKLENMEZ, kayma birikmez.
    TickType_t lastWake = static_cast<TickType_t>(_lastWakeTicks);
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(_period.ms));
    _lastWakeTicks = static_cast<uint32_t>(lastWake);

    _cycleStartUs = static_cast<uint32_t>(esp_timer_get_time());
}

core::ErrCode createAll(const TaskDef* table, uint8_t count)
{
    if (table == nullptr || count == 0)
    {
        core::diag::log(core::LogLevel::CRITICAL, core::ErrCode::SYS_TASK_CREATE_FAILED, 0,
                        "task tablosu bos");
        return core::ErrCode::SYS_TASK_CREATE_FAILED;
    }

    core::ErrCode overall = core::ErrCode::OK;

    for (uint8_t i = 0; i < count; ++i)
    {
        const TaskDef& d = table[i];

        if (d.entry == nullptr)
        {
            core::diag::log(core::LogLevel::CRITICAL, core::ErrCode::SYS_TASK_CREATE_FAILED,
                            static_cast<int32_t>(d.id), "task giris fonksiyonu tanimsiz");
            overall = core::ErrCode::SYS_TASK_CREATE_FAILED;
            continue;
        }

        // stackBytes BAYT cinsindendir (ESP-IDF), word değil.
        const BaseType_t rc = xTaskCreatePinnedToCore(d.entry, d.name, d.stackBytes, nullptr,
                                                      d.priority, nullptr,
                                                      static_cast<BaseType_t>(d.core));

        if (rc != pdPASS)
        {
            // Bir task'ın oluşturulamaması CRITICAL'dır: o işlevsellik olmadan
            // sistem güvenli çalışamaz. Boot yürütücüsü bunu zorunlu aşama
            // başarısızlığı olarak SAFE moda çevirir.
            core::diag::log(core::LogLevel::CRITICAL, core::ErrCode::SYS_TASK_CREATE_FAILED,
                            static_cast<int32_t>(d.id), "task olusturulamadi");
            overall = core::ErrCode::SYS_TASK_CREATE_FAILED;
            continue;
        }

        core::diag::log(core::LogLevel::INFO, core::ErrCode::OK, static_cast<int32_t>(d.core),
                        d.name);
    }

    return overall;
}

} // namespace tasks
