#pragma once

// Task tanım tablosu — TASK-011
//
// ARCHITECTURE §6.1 tablosunun tek doğruluk kaynağı. Periyot, öncelik, stack
// ve çekirdek burada tanımlanır; başka hiçbir yerde tekrarlanmaz.
//
// STACK BİRİMİ — DİKKAT: ESP-IDF FreeRTOS stack boyutunu **BAYT** olarak alır
// ("Note that this differs from vanilla FreeRTOS"). Word sanılırsa stack'ler
// 4 kat küçük oluşur ve sahada rastgele taşma yaşanır.
//
// BOOT SIRASI BAĞIMLILIĞI: task'lar boot Aşama 8'de, yani config/FS/OLED/
// sensör/Wi-Fi aşamalarından SONRA oluşturulur. Çalışmaya başladıklarında tüm
// önkoşullar sağlanmıştır; bu yüzden ayrı bir hazır-olma senkronizasyonu
// (EventGroup) gerekmez. Task'lar ileride daha erken oluşturulacak olursa
// bu varsayım geçersiz kalır ve senkronizasyon eklenmelidir.

#include <stdint.h>

#include "core/TaskRegistry.h"
#include "core/Time.h"
#include "core/WatchdogGuard.h"

namespace tasks {

/// Task giriş fonksiyonu — FreeRTOS imzası.
using TaskEntry = void (*)(void*);

/// Bir task'ın tüm yapılandırması.
struct TaskDef
{
    core::TaskId    id;
    const char*     name;
    TaskEntry       entry;
    core::Duration  period;
    core::TaskClass taskClass;
    uint32_t        stackBytes;  ///< BAYT (ESP-IDF), word değil
    uint8_t         priority;
    uint8_t         core;        ///< 0 = ağ ağırlıklı, 1 = kontrol ağırlıklı
};

// ---------------------------------------------------------------------------
// Çekirdek dağılımı — ARCHITECTURE §6.2
//
//   Core 0: Wi-Fi / lwIP yığını + AsyncTCP + net + store
//   Core 1: app_core + io_sense + ui
//
// Gerekçe: Wi-Fi yığını öngörülemeyen süreler harcar. Güvenlik ve aktüatör
// kontrolü bu belirsizlikten yalıtılmalıdır.
// ---------------------------------------------------------------------------
constexpr uint8_t CORE_NETWORK = 0;
constexpr uint8_t CORE_CONTROL = 1;

// ---------------------------------------------------------------------------
// Öncelikler — ARCHITECTURE §6.3
//
// `app_core` EN YÜKSEK: güvenlik kararları gecikmemelidir.
// Mevcut projede en yüksek öncelik LOGLAMA task'ındaydı — ters çevrildi.
// ---------------------------------------------------------------------------
constexpr uint8_t PRIO_APP_CORE = 4;
constexpr uint8_t PRIO_IO_SENSE = 3;
constexpr uint8_t PRIO_NET      = 2;
constexpr uint8_t PRIO_UI       = 2;
constexpr uint8_t PRIO_STORE    = 1;

// ---------------------------------------------------------------------------
// Stack boyutları (BAYT) — ARCHITECTURE §6.1
//
// Bunlar başlangıç TAHMİNİDİR. TASK-062 watermark ölçümüyle düzeltecektir:
// önce cömert, sonra ölçüme göre kısılır. `TaskRegistry::minFreeStackBytes`
// bu ölçümü sağlar.
// ---------------------------------------------------------------------------
constexpr uint32_t STACK_APP_CORE = 4096;
constexpr uint32_t STACK_IO_SENSE = 3072;
constexpr uint32_t STACK_NET      = 5120;
constexpr uint32_t STACK_UI       = 3584;
constexpr uint32_t STACK_STORE    = 4096;

/// Kaç task oluşturulacak. ARCHITECTURE §6.4: "her özellik için bir task"
/// yaklaşımı KULLANILMAZ; yeni task eklemeden önce o bölüm okunmalıdır.
constexpr uint8_t TASK_COUNT = static_cast<uint8_t>(core::TaskId::COUNT);

} // namespace tasks
