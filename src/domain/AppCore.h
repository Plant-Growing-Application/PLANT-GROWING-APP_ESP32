#pragma once

// Sistemin karar merkezi — TASK-033
//
// ── DEĞERLENDİRME DÖNGÜSÜ (ARCHITECTURE §11.1) — SIRA DEĞİŞTİRİLEMEZ ────────
//
//   0. acil durdurma yolunu tüket        ◀── snapshot'tan bile ÖNCE
//   1. snapshot al                       ◀── bir kez, döngü boyunca aynı görüntü
//   2. komut kuyruğunu boşalt (en fazla N)
//   3. SafetyMonitor.evaluate()          ◀── HER ZAMAN otomasyondan önce
//   4. [otomasyon — TASK-057, şimdilik boş]
//   5. komutları uygula
//   6. ActuatorManager.apply()
//   7. yayınla
//
// **Adım 3 asla adım 4'ten sonra gelmez.** Otomasyon, güvenlik değerlendirmesi
// yapılmamış bir state üzerinde karar veremez.
//
// ── TASK'TAN AYRI ───────────────────────────────────────────────────────────
// Bu modül FreeRTOS bilmez; `now` dışarıdan verilir. `tasks/AppCoreTask.cpp`
// yalnızca ince bir sarmalayıcıdır. Güvenlik döngüsünün host tarafında
// sahte zamanla test edilebilir olması pazarlıksızdır (TASK-064).

#include <stdint.h>

#include "core/Config.h"
#include "core/ErrorCodes.h"
#include "core/Time.h"

namespace domain {
namespace appcore {

/// Döngü başına işlenecek en fazla komut sayısı.
///
/// Kuyruğu tamamen boşaltmak, 16 komutluk bir selde güvenlik döngüsünün
/// periyodunu bozardı — ve güvenlik döngüsünün gecikmesi komut
/// gecikmesinden ciddidir. 4 komut/döngü × 10 döngü/sn = 40 komut/sn,
/// bir operatörün üretebileceğinin çok üstünde.
constexpr uint8_t MAX_COMMANDS_PER_CYCLE = 4;

/// Bu süreden eski komutlar atılır: kuyrukta bekleyen bir "pompayı aç"
/// komutu, koşullar değiştikten sonra uygulanmamalıdır.
constexpr uint32_t COMMAND_MAX_AGE_MS = 3000;

/// Döngü işi bu süreyi aşarsa WARNING loglanır (bir kez).
constexpr uint32_t CYCLE_BUDGET_US = 30000;   // 30 ms — 100 ms periyodun %30'u

/// Alt sistemleri bağlar ve güvenlik zincirini kurar.
///
/// `ActuatorManager`'a güvenlik izni olarak `SafetyMonitor::permits`
/// verilir; `SystemSupervisor`'a güvenli hâl işleyicisi olarak aktüatör
/// kapatma bağlanır. Bu bağlantılar kurulmadan döngü çalıştırılmamalıdır.
core::ErrCode begin(const core::Config& cfg);

/// Bir değerlendirme çevrimi. `now` monotonik zamandır.
void tick(core::Millis now);

/// Son çevrimin iş süresi (µs) — tanılama.
uint32_t lastCycleUs();

/// Bütçe aşımı sayısı.
uint32_t budgetOverruns();

} // namespace appcore
} // namespace domain
