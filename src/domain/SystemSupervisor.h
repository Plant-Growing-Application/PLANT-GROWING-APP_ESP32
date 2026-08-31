#pragma once

// Sistem modu ve task sağlığı denetimi — TASK-012
//
// İKİ İŞİ VAR:
//   1. Sistem modunu yönetmek (BOOTING/RUNNING/DEGRADED/SAFE/EMERGENCY)
//   2. Task heartbeat'lerini izleyip donanım watchdog'unu BEKLEMEDEN
//      yavaşlamayı yakalamak
//
// KATMAN: `domain/` — yalnızca `core/`'a bağımlıdır (D5). Donanıma dokunmaz,
// servisleri tanımaz. Bu sayede host tarafında test edilebilir (TASK-064).
//
// ── ÖNEMLİ SINIR ────────────────────────────────────────────────────────────
// Bu modül `app_core` task'ı İÇİNDE çalışır (ARCHITECTURE §2.14). Dolayısıyla
// `app_core` kilitlenirse onu izleyecek kod da çalışmaz. Bu boşluk KAPATILAMAZ;
// `app_core` için tek koruma donanım watchdog'udur (TASK-009, 8 sn).
//
// Diğer dört task için buradaki izleme çok daha hızlıdır: 300 ms – 5 sn.
// ────────────────────────────────────────────────────────────────────────────

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/SystemState.h"
#include "core/TaskRegistry.h"
#include "core/Time.h"

namespace domain {

/// DEGRADED modda kalma nedenleri.
///
/// Maske sıfırlanınca RUNNING'e dönülür. Her bitin temizlenme koşulu AÇIKTIR —
/// belirsiz bırakılırsa sistem kalıcı olarak DEGRADED'da takılır ve kimse
/// nedenini bilemez.
enum DegradedReason : uint16_t
{
    DEGRADED_NONE = 0,

    /// Boot'ta zorunlu olmayan bir aşama başarısız oldu.
    /// **Kendiliğinden temizlenmez:** boot'ta mount edilemeyen bir dosya
    /// sistemi çalışma zamanında kendini düzeltmez. Yalnızca yeniden başlatma
    /// temizler. Sahte bir "düzeldi" sinyali vermek yanıltıcı olurdu.
    DEGRADED_BOOT_STAGE = 1u << 0,

    /// Bir task'ın heartbeat'i bayatladı.
    /// Tüm kayıtlı task'lar yeniden atmaya başlayınca temizlenir.
    DEGRADED_TASK_STALL = 1u << 1,

    /// `Diagnostics`'te aktif hata var.
    /// `activeFaultCount() == 0` olunca temizlenir.
    DEGRADED_ACTIVE_FAULT = 1u << 2,
};

/// Aktüatörleri güvenli duruma alan işleyici.
///
/// NEDEN FONKSİYON İŞARETÇİSİ: aktüatörleri kapatmak `ActuatorManager`'ın
/// işidir (TASK-029) ve o modül henüz yazılmadı. İşleyici, "önce aktüatör
/// güvenli duruma, SONRA mod değişimi" sırasını **yapısal olarak** garanti
/// eder — sıra yoruma bırakılmaz. Ayrıca sahte bir işleyiciyle host tarafında
/// test edilebilir.
using SafeStateHandler = void (*)(core::ErrCode reason);

namespace supervisor {

/// Denetleyiciyi başlatır ve RTC belleğindeki yeniden başlatma kaydını okur.
/// Boot'un core aşamasında çağrılır.
void begin();

/// Aktüatörleri güvenli duruma alacak işleyiciyi kaydeder.
/// TASK-033 (`app_core`) bunu `ActuatorManager::forceAllOff`'a bağlayacaktır.
void setSafeStateHandler(SafeStateHandler handler);

/// Boot sonucunu uygular: modu ayarlar, başarısız aşama varsa DEGRADED nedeni
/// işaretler. TASK-010'un döndürdüğü mod ile çağrılır.
void applyBootResult(core::SystemMode bootMode, uint8_t failedStageCount);

/// Her `app_core` döngüsünde çağrılır: heartbeat'leri kontrol eder, DEGRADED
/// nedenlerini günceller, bekleyen yeniden başlatmayı yürütür ve `system`
/// alt-state'ini yayınlar.
void tick(core::Millis now);

/// Mevcut sistem modu.
core::SystemMode mode();

/// Mod değiştirmeyi dener.
///
/// Geçiş tablosuna uymayan istek **REDDEDİLİR** ve CRITICAL loglanır.
/// (Bu, `StateStore`'un tek yazar ihlalindeki davranışından bilinçli olarak
/// farklıdır: orada yazmayı iptal etmek eksik state yaratırdı; burada
/// geçersiz geçişi uygulamak sistemi tutarsız bir moda sokar. Bilinmeyen bir
/// moda geçmektense bilinen modda kalmak yeğdir.)
///
/// @return true = geçiş uygulandı
bool setMode(core::SystemMode target, core::ErrCode reason);

/// İki mod arasındaki geçişe izin var mı? Saf fonksiyon — host'ta test edilebilir.
bool canTransition(core::SystemMode from, core::SystemMode to);

/// Aktif DEGRADED nedenleri maskesi.
uint16_t degradedReasons();

// --- Kontrollü yeniden başlatma ---------------------------------------------

/// Yeniden başlatma talep eder.
///
/// Anında reset veri kaybı yaratır: bekleyen flash yazmaları tamamlanamaz ve
/// istemciye yanıt gönderilemez. Bu yüzden talep GECİKMELİDİR — `tick()`
/// süre dolunca aktüatörleri güvenli duruma alır, nedeni RTC belleğine yazar
/// ve reset eder.
///
/// @param delay varsayılan 500 ms: HTTP yanıtının gitmesine ve `store`
///              task'ının kuyruğunu boşaltmasına yetecek kadar
void requestRestart(core::ErrCode reason, core::Duration delay = core::millisecs(500));

/// Bekleyen yeniden başlatma var mı?
bool restartPending();

/// Önceki oturumda kontrollü yeniden başlatma yapıldıysa nedeni; yoksa
/// `ErrCode::OK`.
///
/// RTC belleğinde (`RTC_NOINIT_ATTR`) saklanır: yazılım reset'inde korunur,
/// güç kesintisinde kaybolur. Güç kesintisinde zaten `ResetReason::POWER_ON`
/// okunacağı için bilgi kaybı yoktur. NVS'e bağımlı olmadan çözüldü
/// (`domain/` → `services/` bağımlılığı yasak).
core::ErrCode previousRestartReason();

} // namespace supervisor
} // namespace domain
