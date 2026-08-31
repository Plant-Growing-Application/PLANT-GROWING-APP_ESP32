#pragma once

// Periyodik task iskeleti — TASK-011
//
// DÖNGÜ SIRASINI YAPISAL OLARAK ZORLAR (ARCHITECTURE §6.5):
//
//     iş  →  heartbeat  →  watchdog besleme  →  periyodik bekleme
//
// Kullanım:
//
//     void appCoreTask(void*)
//     {
//         TaskRunner runner(TaskId::APP_CORE, TaskClass::CONTROL, millisecs(100));
//         runner.begin();                 // WDT kaydı + registry kaydı
//         for (;;)
//         {
//             ... iş ...
//             runner.endCycle();          // heartbeat + besleme + bekleme
//         }
//     }
//
// NEDEN BESLEME EN SONDA: döngü ortasında besleme, task'ın gerçekten
// ilerlediğini KANITLAMAZ. Mevcut projede `Task_WiFiMonitor` beslemeyi
// 5 saniye bloklayan `connect()` çağrısından sonra yapıyordu — bloklama
// watchdog tarafından hiç görülmedi.
//
// NEDEN `vTaskDelayUntil`: `vTaskDelay(period)` iş süresini periyoda EKLER
// (100 ms hedef + 30 ms iş = 130 ms gerçek periyot) ve kayma birikir.
// Mutlak uyanma zamanı kullanıldığında periyot sabit kalır — güvenlik
// döngüsünün zamanlaması buna bağlıdır.

#include <stdint.h>

#include "core/TaskRegistry.h"
#include "core/Time.h"
#include "core/WatchdogGuard.h"

namespace tasks {

// İleri bildirim: `createAll` yalnızca işaretçi alır, tam tanıma ihtiyaç yok.
// Tabloyu kuran çağıran `TaskConfig.h`'ı kendisi include eder
// (CODING_STANDARDS §1: her dosya yalnızca kullandığını include eder).
struct TaskDef;

class TaskRunner
{
public:
    TaskRunner(core::TaskId id, core::TaskClass cls, core::Duration period);

    /// Task'ın kendini kaydetmesi: TWDT aboneliği + sağlık kaydı.
    /// **Task'ın kendi başlangıcında** çağrılır (ARCHITECTURE §6.5).
    ///
    /// TWDT kaydı başarısız olursa CRITICAL loglanır — bir task'ın izlenmiyor
    /// olması sessizce geçilemez (mevcut projede `Task_SensorLogger` hiç
    /// kaydolmamıştı ve kimse fark etmemişti).
    void begin();

    /// Döngü sonu: heartbeat → watchdog besleme → periyodik bekleme.
    /// Döngünün EN SON ifadesi olmalıdır.
    void endCycle();

    /// Bu task'ın kaç döngü periyodu aştığı.
    uint32_t overrunCount() const { return _overruns; }

private:
    core::TaskId    _id;
    core::TaskClass _class;
    core::Duration  _period;
    uint32_t        _lastWakeTicks;  ///< vTaskDelayUntil için mutlak zaman
    uint32_t        _cycleStartUs;
    uint32_t        _cycles;
    uint32_t        _overruns;
    bool            _feedFailedLogged;
};

/// Beş task'ı çekirdeğe sabitlenmiş olarak oluşturur.
///
/// Boot Aşama 8'den (`TASK_CREATION`) çağrılır — boot wiring bağlayacaktır (bkz. ISSUE-013).
///
/// Task oluşturma başarısızlığı CRITICAL bir hatadır: sistem o işlevsellik
/// olmadan güvenli çalışamaz. Hata döndürülür ve boot yürütücüsü zorunlu
/// aşama başarısızlığı olarak SAFE moda geçirir.
///
/// @return ErrCode::OK veya SYS_TASK_CREATE_FAILED
core::ErrCode createAll(const TaskDef* table, uint8_t count);

} // namespace tasks
