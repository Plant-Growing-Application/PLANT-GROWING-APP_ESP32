#pragma once

// Task Watchdog sarmalayıcısı — TASK-009
//
// İKİ KATMANLI KORUMA (ARCHITECTURE §6.5):
//
//   Donanım katı  : TWDT — TAM KİLİTLENMEYE karşı, tek global süre (8 sn)
//   Uygulama katı : heartbeat — YAVAŞLAMAYA karşı, task başına yumuşak son
//                   tarih (TASK-011 kaydeder, TASK-012 izler)
//
// NEDEN İKİ KATMAN: ESP-IDF 4.4'te TWDT tek global zamanlayıcıdır; task başına
// farklı timeout DESTEKLENMEZ (bkz. ISSUE-011). Kontrol task'ının 100 ms'lik
// periyodunu 8 sn'lik bir watchdog'la izlemek çok gevşektir — bu boşluğu
// uygulama katmanındaki heartbeat kapatır.
//
// ÖNEMLİ ORTAM GERÇEĞİ: TWDT `setup()` çalışmadan ÖNCE IDF tarafından zaten
// başlatılmıştır (sdkconfig: TIMEOUT_S=5, PANIC=y). Bu modülün `begin()`
// çağrısı bir ilk kurulum değil, YENİDEN YAPILANDIRMAdır. Sistem ilk komuttan
// itibaren korumalıdır.
//
// RESET NEDENİ BURADA YOK: `diag::captureResetReason()` (TASK-005) bu işi
// yapar ve WDT/panic kaynaklı reset'i CRITICAL olarak loglar. Burada
// tekrarlanmaz (ARCHITECTURE P7 — ikiz kod yasağı).

#include <stdint.h>

#include "ErrorCodes.h"
#include "Time.h"

namespace core {

/// Task'ın zamanlama sınıfı. TWDT tek global süre kullandığı için bu sınıflar
/// **yumuşak son tarihi** belirler: heartbeat izleyicisi (TASK-012) bir task'ın
/// bu süre boyunca ilerlememesini yavaşlama olarak raporlar.
enum class TaskClass : uint8_t
{
    CONTROL = 0,  ///< app_core — 100 ms periyot, güvenlik döngüsü
    SENSING = 1,  ///< io_sense — 250 ms periyot
    UI      = 2,  ///< ui — 50 ms periyot
    NETWORK = 3,  ///< net — 100 ms periyot ama olay güdümlü, değişken
    STORAGE = 4,  ///< store — olay güdümlü, flash yazması uzun sürebilir
};

/// Sınıfa göre yumuşak son tarih: task bu süre boyunca ilerlemezse yavaşlamış
/// sayılır. Değerler ARCHITECTURE §6.1 periyotlarının ~3 katıdır (§6.5:
/// "3 periyot boyunca artmazsa").
constexpr Duration softDeadline(TaskClass c)
{
    return c == TaskClass::CONTROL   ? millisecs(400)
         : c == TaskClass::SENSING   ? millisecs(1000)
         : c == TaskClass::UI        ? millisecs(300)
         : c == TaskClass::NETWORK   ? millisecs(3000)
         :                             millisecs(5000);  // STORAGE
}

namespace wdt {

/// Varsayılan TWDT süresi.
///
/// GÜVENLİK HESABI: `app_core` kilitlenirse ve pompa açıksa, pompa
/// (timeout + reset + boot Aşama 1) kadar kontrolsüz çalışır. 8 sn seçildi:
/// en uzun meşru işlemin (flash toplu yazma < 1 sn) yaklaşık 8 katı pay
/// bırakırken, kontrolsüz pompa süresini ~8.5 sn ile sınırlar.
constexpr Duration DEFAULT_TIMEOUT = seconds(8);

/// TWDT'yi yeniden yapılandırır. **Task oluşturulmadan önce** çağrılmalıdır
/// (ARCHITECTURE §7.1 Aşama 0) — böylece tüm abonelikler bilinen bir süreyle
/// yapılır.
///
/// Mevcut projedeki hata tam buydu: init task'lardan SONRA çağrılıyordu ve
/// hangi sürenin geçerli olduğu belirsizdi.
///
/// @param timeout saniye çözünürlüğüne yuvarlanır (TWDT saniye alır)
/// @param panic   true = zaman aşımında donanımsal reset. Yalnızca loglamak
///                yeterli değildir: kilitlenmiş bir sistem pompayı açık bırakır.
/// @return ErrCode::OK veya SYS_BOOT_STAGE_FAILED
ErrCode begin(Duration timeout = DEFAULT_TIMEOUT, bool panic = true);

/// Çağıran task'ı TWDT'ye kaydeder.
///
/// KURAL (ARCHITECTURE §6.5): her task **kendi başlangıcında** kendini
/// kaydeder; başka bir task adına kayıt yapılmaz. Kayıt, task'ın gerçekten
/// çalışmaya başladığını kanıtlar.
///
/// @return ErrCode::OK, veya SYS_TASK_CREATE_FAILED (TWDT hazır değil / bellek)
ErrCode subscribe();

/// Çağıran task'ın kaydını siler. Task sonlanacaksa **zorunludur**;
/// aksi halde TWDT sonlanmış bir task için yanlış alarm üretir.
ErrCode unsubscribe();

/// Watchdog'u besler.
///
/// KURAL: **yalnızca döngünün en sonunda**, tüm iş bittikten sonra çağrılır.
/// Döngü ortasında besleme, task'ın gerçekten ilerlediğini kanıtlamaz ve
/// watchdog'u anlamsız kılar.
///
/// Mevcut projede `Task_WiFiMonitor` beslemeyi 5 saniye bloklayan `connect()`
/// çağrısından SONRA yapıyordu — bloklama watchdog tarafından hiç görülmedi.
ErrCode feed();

/// Çağıran task TWDT'ye kayıtlı mı?
bool isSubscribed();

/// Bu modül üzerinden yapılmış aktif abonelik sayısı.
/// Beklenen: 5 (ARCHITECTURE §6.1 task tablosu).
uint8_t subscriberCount();

/// Yapılandırılmış zaman aşımı süresi.
Duration timeout();

/// TWDT yapılandırıldı mı?
bool isConfigured();

} // namespace wdt
} // namespace core
