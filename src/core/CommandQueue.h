#pragma once

// Komut kuyruğu — TASK-008
//
// Arayüz katmanı ile domain katmanı arasına TASK SINIRI koyar (ARCHITECTURE §3.3):
//
//   [Web / OLED]  --post()-->  [kuyruk]  --receive()-->  [app_core]  --> aktüatör
//                                                         ^
//                                            güvenlik burada uygulanır
//
// AsyncTCP callback'i asla röle sürmez; yalnızca kuyruğa yazar ve hemen döner.
//
// ÇOK YAZAR → TEK OKUYUCU: web callback'i, `ui` task'ı ve sistem kaynakları
// yazar; yalnızca `app_core` task'ı okur.

#include <stdint.h>

#include "Command.h"
#include "ErrorCodes.h"

namespace core {

/// Kuyruk kapasitesi. 16 × 20 bayt = 320 bayt.
/// Çok küçük → komut kaybı; çok büyük → RAM israfı ve eskimiş komut birikmesi.
constexpr uint8_t COMMAND_QUEUE_CAPACITY = 16;

namespace cmdq {

/// Kuyruğu oluşturur. Boot'un erken aşamalarında çağrılmalıdır
/// (ARCHITECTURE §7.1 Aşama 2). Tekrar çağrılması güvenlidir.
///
/// @return ErrCode::OK veya SYS_BOOT_STAGE_FAILED
ErrCode begin();

/// Komutu kuyruğa koyar. **ASLA BLOKLAMAZ** — zaman aşımı sıfırdır.
///
/// AsyncTCP callback bağlamından çağrılabilir (ARCHITECTURE §14.6): bloklama
/// tüm web arayüzünü dondururdu.
///
/// `issuedAt` alanı çağıran tarafından doldurulmamışsa burada doldurulur.
///
/// @return ACCEPTED  → kuyruğa alındı (uygulandığı anlamına GELMEZ)
///         BUSY      → kuyruk dolu; en eski komut düşürülmez, yeni komut reddedilir
///         REJECTED_INVALID → tip NONE veya kuyruk başlatılmamış
CommandResult post(const Command& cmd);

/// Kuyruktan bir komut alır. Bloklamaz.
/// Yalnızca `app_core` task'ından çağrılmalıdır (tek okuyucu kuralı).
///
/// @return true = komut alındı
bool receive(Command& out);

/// Kuyrukta bekleyen komut sayısı.
uint8_t pending();

/// Kuyruk dolu olduğu için reddedilen toplam komut sayısı.
/// Sıfırdan farklıysa ya `app_core` yavaşlamış ya da komut seli var.
uint32_t rejectedCount();

// ---------------------------------------------------------------------------
// Acil durdurma — GARANTİLİ YOL
//
// Acil durdurma kuyruktan GEÇMEZ. Atomik bir bayrak kullanılır, böylece kuyruk
// tamamen dolu olsa bile komut ulaşır (ARCHITECTURE §12.3).
//
// `app_core` her döngüde ÖNCE bu bayrağı kontrol eder, SONRA kuyruğu boşaltır.
// ---------------------------------------------------------------------------

/// Acil durdurma talep eder. Kilitsiz, bloklamayan; kuyruk doluluğundan
/// bağımsızdır. Aynı döngüde birden fazla çağrı tek talebe indirgenir.
void postEmergencyStop(CommandSource source, ErrCode reason);

/// Bekleyen acil durdurma talebini alır ve bayrağı temizler.
/// Yalnızca `app_core` çağırmalıdır.
///
/// @param outSource talebi kimin verdiği
/// @param outReason neden kodu
/// @return true = bekleyen talep vardı
bool takeEmergencyStop(CommandSource& outSource, ErrCode& outReason);

/// Bekleyen acil durdurma talebi var mı? (Bayrağı temizlemez.)
bool emergencyStopPending();

/// Kuyruğu ve acil durum bayrağını boşaltır (test ve yeniden başlatma için).
void reset();

} // namespace cmdq
} // namespace core
