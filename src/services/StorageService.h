#pragma once

// Kalıcılaştırma servisi ve yazma kuyruğu — TASK-059
//
// ── BLOKLAMAMA GARANTİSİ ────────────────────────────────────────────────────
//
//   ConfigService.persist()  ─┐
//   HistoryStore.append()    ─┼──▶ yazma kuyruğu ──▶ store task ──▶ flash
//   kritik log               ─┘     (bloklamayan)    (en düşük öncelik)
//
// Flash yazma yavaş ve **değişken sürelidir**. `app_core` veya bir AsyncTCP
// callback'i bunu ASLA beklememelidir. Eski projede EEPROM yazma Wi-Fi
// event handler'ından yapılıyordu — kritik bir bağlamda yavaş bir işlem.
//
// ── KUYRUK DOLDUĞUNDA ───────────────────────────────────────────────────────
//   config > kritik log > geçmiş
//   Geçmiş DÜŞÜRÜLÜR, config ASLA. Bir örneği kaybetmek grafikte bir
//   boşluktur; ayarı kaybetmek kullanıcının yeniden girmesidir ve fark
//   edilmesi zordur.

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/Time.h"

namespace services {
namespace storage {

/// Kuyruk kapasitesi. Son slot config/kritik için REZERVdir — geçmiş
/// kayıtları kuyruğu doldurup config'i dışarıda bırakamaz.
constexpr uint8_t QUEUE_LEN       = 8;
constexpr uint8_t HISTORY_MAX_USE = QUEUE_LEN - 1;

/// Kuyruk boşken task bu süre sonra uyanıp heartbeat besler. Sonsuz
/// bloklama watchdog'u tetiklerdi.
constexpr uint32_t WAKE_TIMEOUT_MS = 1000u;

enum class WriteKind : uint8_t
{
    NONE            = 0,
    CONFIG_PERSIST  = 1,  ///< ASLA düşürülmez
    HISTORY_SAMPLE  = 2,  ///< kuyruk dolarsa düşürülür
};

struct WriteRequest
{
    WriteKind kind;
    uint8_t   reserved[3];
    uint32_t  param;
};

core::ErrCode begin();

/// İsteği kuyruğa koyar — **BLOKLAMAZ**.
///
/// @return `true` = kuyruğa alındı; `false` = düşürüldü (sayılır)
bool post(WriteKind kind, uint32_t param = 0);

/// Bir çevrim: kuyruğu tüket, periyodik örnek zamanı geldiyse kaydet.
void tick(core::Millis now);

// --- Teşhis ------------------------------------------------------------------

uint32_t droppedRequests();
uint32_t processedRequests();

} // namespace storage
} // namespace services
