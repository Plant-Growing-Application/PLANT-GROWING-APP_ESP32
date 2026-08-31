#pragma once

// Teşhis ve loglama — TASK-005
//
// Mevcut sistemdeki dağınık `Serial.println` yaklaşımının yerini alır.
// Üç işi vardır:
//
//   1. Seviyeli olay kaydı (halka tampon, RAM)
//   2. Aktif hata takibi (raise/clear) — UI ve API için
//   3. Boot raporu ve reset nedeni saklama
//
// TASARIM KARARI: kayıtta SERBEST METİN SAKLANMAZ. Kayıt `ErrCode` + sayısal
// bağlam (`detail`) taşır; metin yalnızca seri porta yazılır. Kod→metin eşlemesi
// sunum katmanının işidir (TASK-049 frontend, TASK-052 OLED) — orada
// yerelleştirilebilir ve cihaz RAM'i harcanmaz.
//
// Bu header platform bağımsızdır (D5): ESP-IDF ve FreeRTOS yalnızca .cpp içinde
// kullanılır.

#include <stdint.h>

#include "BootReport.h"
#include "ErrorCodes.h"
#include "Time.h"
#include "Types.h"

namespace core {

/// Halka tamponda tutulan olay sayısı. 64 × 12 bayt = 768 bayt.
constexpr uint8_t LOG_RING_CAPACITY = 64;

/// Aynı anda izlenebilecek farklı aktif hata sayısı.
constexpr uint8_t MAX_ACTIVE_FAULTS = 16;

/// Tek bir log kaydı — sabit boyutlu, dinamik ayırma yok.
struct LogRecord
{
    Millis   timestamp;  ///< monotonik (duvar saati DEĞİL — Z4)
    ErrCode  code;
    LogLevel level;
    uint8_t  reserved;   ///< hizalama; ileride kullanılmak üzere sıfır
    int32_t  detail;     ///< bağlam: sensör kimliği, ölçülen değer, alt-neden
};

/// Aktif hataların anlık özeti — UI durum çubuğu ve API için.
struct FaultSummary
{
    uint8_t  count;          ///< aktif farklı hata sayısı
    uint16_t subsystemMask;  ///< hangi alt sistemlerde hata var (bit = Subsystem)
    bool     overflow;       ///< kapasite aşıldı, bazı hatalar izlenemiyor
    ErrCode  faults[MAX_ACTIVE_FAULTS];
};

// ---------------------------------------------------------------------------
// Diagnostics — süreç genelinde tek örnek
//
// Singleton değil, serbest fonksiyon arayüzü: test edilebilirlik için durum
// `reset()` ile temizlenebilir.
// ---------------------------------------------------------------------------
namespace diag {

/// Mutex'i ve tamponları hazırlar. Boot'un ilk adımlarında çağrılmalıdır
/// (ARCHITECTURE §7.1 Aşama 2). Tekrar çağrılması güvenlidir.
///
/// @return ErrCode::OK veya SYS_BOOT_STAGE_FAILED (mutex oluşturulamadı)
ErrCode begin();

/// Olay kaydeder ve (etkinse) seri porta yazar.
///
/// @param detail sayısal bağlam — sensör kimliği, ölçülen değer, alt-neden
/// @param msg    YALNIZCA seri porta yazılır, SAKLANMAZ; nullptr olabilir
///
/// ISR'den çağrılmamalıdır (CODING_STANDARDS §6). ISR bağlamından çağrılırsa
/// kayıt düşürülür ve `droppedFromIsr()` sayacı artar.
void log(LogLevel level, ErrCode code, int32_t detail = 0, const char* msg = nullptr);

/// Hatayı aktif olarak işaretler. Zaten aktifse tekrar kaydetmez —
/// bu, her döngüde tekrarlanan bir hatanın log selini önler.
///
/// @return true = yeni aktifleşti (çağıran isterse ek işlem yapabilir)
bool raise(ErrCode code, int32_t detail = 0);

/// Hatayı aktif listesinden çıkarır. Aktif değilse hiçbir şey yapmaz.
/// @return true = gerçekten aktifti ve temizlendi
bool clear(ErrCode code);

/// Aktif hataların anlık kopyası. Çağıran kilit tutmaz.
void activeFaults(FaultSummary& out);

/// Aktif farklı hata sayısı — O(1). UI durum çubuğu 20 Hz'de çağırır.
uint8_t activeFaultCount();

/// Belirli bir hata aktif mi?
bool isActive(ErrCode code);

/// Son kayıtları en yeniden en eskiye doğru kopyalar.
/// @param out     hedef dizi
/// @param maxOut  `out` kapasitesi
/// @return kopyalanan kayıt sayısı
uint8_t recent(LogRecord* out, uint8_t maxOut);

/// Tampona hiç yazılmış toplam kayıt sayısı (taşanlar dahil).
uint32_t totalRecords();

/// ISR bağlamından çağrıldığı için düşürülen kayıt sayısı.
/// Sıfırdan farklıysa bir yerde kural ihlali var demektir.
uint32_t droppedFromIsr();

/// Seri port çıktısı için en düşük seviye. Bu seviyenin altındakiler yalnızca
/// halka tampona yazılır. `CRITICAL`'ın üstüne ayarlanarak çıktı tamamen kapatılır.
void setSerialLevel(LogLevel minLevel);

// --- Boot raporu ve reset nedeni ------------------------------------------

/// Donanımdan reset nedenini okur ve saklar. Watchdog kaynaklı reset
/// CRITICAL olarak loglanır (ARCHITECTURE §16.3).
ResetReason captureResetReason();

/// Saklanan reset nedeni.
ResetReason resetReason();

/// Boot raporuna yazmak için erişim. TASK-010 doldurur.
BootReport& bootReport();

/// Tüm durumu temizler (test ve fabrika ayarları için).
void reset();

} // namespace diag
} // namespace core
