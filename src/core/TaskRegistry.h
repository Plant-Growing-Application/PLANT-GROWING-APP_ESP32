#pragma once

// Task sağlık kaydı — TASK-011
//
// NEDEN `core/` İÇİNDE:
//   YAZAN : TaskRunner  (src/tasks/)  — her döngüde heartbeat
//   OKUYAN: SystemSupervisor (src/domain/, TASK-012) — bayatlama izleme
//
//   `domain/` yalnızca `core/`'a bağımlı olabilir (D5); `tasks/`'i include
//   etmesi katman ihlali olurdu. Depo bu yüzden `core/` içindedir.
//
// NEDEN `SystemState` İÇİNDE DEĞİL: state'in her alt-bölümünün TEK yazarı
// vardır (P1). Heartbeat'i beş farklı task yazar — state'e koymak tek yazar
// kuralını bozardı.
//
// SENKRONİZASYON: her task YALNIZCA kendi slotuna yazar → çekişme yok.
// Slot başına `std::atomic`; kilitsiz, ~nanosaniye maliyet. Okuyucu tüm
// slotları tarar — atomikler için ideal desen.

#include <stdint.h>

#include "Time.h"
#include "WatchdogGuard.h"

namespace core {

/// Task kimlikleri — ARCHITECTURE §6.1 tablosu.
enum class TaskId : uint8_t
{
    APP_CORE = 0,  ///< güvenlik + otomasyon + aktüatör
    IO_SENSE = 1,  ///< sensör örnekleme
    NET      = 2,  ///< Wi-Fi FSM + zaman
    UI       = 3,  ///< OLED + girdi
    STORE    = 4,  ///< kalıcılaştırma
    COUNT    = 5,
};

/// Tek bir task'ın sağlık kaydı.
struct TaskHealth
{
    Millis    lastBeatAt;       ///< son heartbeat zamanı (monotonik)
    uint32_t  beatCount;        ///< toplam döngü sayısı
    uint32_t  maxLoopUs;        ///< görülen en uzun döngü süresi
    uint32_t  overrunCount;     ///< periyodu aşan döngü sayısı
    uint16_t  minFreeStackBytes;///< stack watermark (ESP-IDF: BAYT)
    TaskClass taskClass;
    uint8_t   registered;
};

namespace taskreg {

/// Kaydı sıfırlar. Task'lar oluşturulmadan önce çağrılmalıdır.
void begin();

/// Task kendini kaydeder. **Task'ın kendi başlangıcında** çağrılır;
/// başka bir task adına kayıt yapılmaz.
void registerSelf(TaskId id, TaskClass cls);

/// Heartbeat yayınlar. `TaskRunner` her döngü sonunda çağırır.
///
/// @param loopUs   bu döngünün süresi (µs)
/// @param overran  döngü periyodu aştı mı
void beat(TaskId id, uint32_t loopUs, bool overran);

/// Stack watermark'ı günceller. Ölçüm maliyeti stack boyutuyla orantılıdır;
/// her döngüde değil, seyrek çağrılır (TaskRunner 64 döngüde bir yapar).
void updateStack(TaskId id, uint16_t freeBytes);

/// Bir task'ın sağlık kaydını kopyalar.
void health(TaskId id, TaskHealth& out);

/// Son heartbeat'ten bu yana geçen süre.
///
/// Bayatlama KARARI burada verilmez — eşik karşılaştırması `SystemSupervisor`
/// (TASK-012) işidir. Burada yalnızca ham veri sağlanır.
Duration sinceLastBeat(TaskId id, Millis now);

/// Kayıtlı task sayısı. Beklenen: 5 (ARCHITECTURE §6.1).
uint8_t registeredCount();

} // namespace taskreg
} // namespace core
