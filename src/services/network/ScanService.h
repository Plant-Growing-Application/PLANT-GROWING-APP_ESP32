#pragma once

// Wi-Fi tarama servisi — TASK-039
//
// ── DÜZELTİLEN ARA-DURUM PROBLEMİ ───────────────────────────────────────────
//   Eski: tarama sürerken sunucu `202 {"status":"scanning"}` döndürüyordu.
//         Frontend bunu dizi sanıp `forEach` çağırıyor → hata →
//         "Ağ taraması yapılamadı!" uyarısı. **İlk tıklama HER ZAMAN
//         başarısız oluyordu.**
//   Yeni: yanıt HER DURUMDA aynı şemada:
//         `{ status, networks[], age }`
//         Farklı durumlarda farklı şekilli yanıt döndürmek istemci
//         hatalarının ana kaynağıdır.

#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/Time.h"

namespace services {
namespace net {
namespace scan {

/// Tampon üst sınırı. Aşan ağlar kesilir ve `truncated()` bunu bildirir.
constexpr uint8_t MAX_RESULTS = 20;
constexpr uint8_t SSID_MAX    = 33;

/// Sonuç bu süreden sonra eskimiş sayılır — 30 saniye önceki bir tarama
/// artık geçerli olmayabilir. Yaş bilgisi yanıtta taşınır.
constexpr uint32_t RESULT_MAX_AGE_MS = 30000u;

enum class ScanState : uint8_t
{
    IDLE    = 0,
    RUNNING = 1,
    DONE    = 2,
    FAILED  = 3,
};

struct ScanEntry
{
    char    ssid[SSID_MAX];
    int8_t  rssi;
    uint8_t channel;
    uint8_t encType;   ///< 0 = açık ağ; şifresiz bağlanma senaryosu için gerekli
    uint8_t reserved;
};

core::ErrCode begin();

/// Taramayı başlatır. Zaten çalışıyorsa `OK` döner ve yeniden başlatmaz.
core::ErrCode start(core::Millis now);

/// `SCAN_DONE` olayı geldiğinde çağrılır — sonuçları kendi tamponumuza
/// kopyalar ve radyo tarafındaki belleği serbest bırakır.
void onScanDone(core::Millis now, uint8_t count);

/// Tarama başarısız oldu.
void onScanFailed(core::Millis now);

/// Emniyet valfi: `SCAN_DONE` olayı hiç gelmezse taramayı düşürür.
void tick(core::Millis now);

ScanState state();
uint8_t   count();
const ScanEntry* results();

/// Sonuçların yaşı (ms). `DONE` değilken 0.
uint32_t ageMs(core::Millis now);

/// Sonuçlar `MAX_RESULTS` nedeniyle kesildi mi?
bool truncated();

} // namespace scan
} // namespace net
} // namespace services
