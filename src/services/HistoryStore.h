#pragma once

// Halka dosya geçmiş deposu — TASK-058
//
// ── SQLite YERİNE HALKA DOSYA (ARCHITECTURE §15.2) ──────────────────────────
//   1. Sorgu ihtiyacı yok — erişim deseni yalnızca "zaman aralığı oku"
//   2. Ciddi flash ve RAM maliyeti
//   3. LittleFS üzerinde B-tree yazması aşınmayı artırır
//   4. Eski projede hiç etkinleştirilmemişti — kaybedilen işlevsellik yok
//
// ── KAPASİTE ────────────────────────────────────────────────────────────────
//   480 KB / 24 bayt = 20 480 kayıt
//   60 sn periyotla  → 14,2 gün · 300 sn periyotla → 71 gün
//
// LittleFS bölümü 896 KB; kalan ~400 KB aşınma dengelemesi ve meta veri için
// serbest bırakıldı — dosya sistemini tavana kadar doldurmak yazma
// performansını çökertir.
//
// ── HALKA KONUMU BİNARY SEARCH İLE BULUNUR ──────────────────────────────────
// Yazma dizinini her kayıtta flash'a yazmak aşınmayı ikiye katlardı; tüm
// dosyayı taramak boot'a ~0,5 sn eklerdi. Bunun yerine `seq` alanı üzerinde
// iki ikili arama (~30 okuma, 720 bayt) yapılır. `seq`in varlık nedeni budur.

#include <stddef.h>
#include <stdint.h>

#include "core/ErrorCodes.h"
#include "core/SystemState.h"
#include "core/Time.h"

namespace services {
namespace history {

constexpr const char* FILE_PATH = "/hist.bin";

constexpr uint8_t  SENSOR_SLOTS  = 6;
constexpr uint32_t RECORD_COUNT  = 20480u;
constexpr uint32_t RECORD_BYTES  = 24u;
constexpr uint32_t FILE_BYTES    = RECORD_COUNT * RECORD_BYTES;   // 480 KB

/// Tek okuma sorgusunun üst sınırı. `/api/history` bunu aşamaz: sınırsız
/// bir aralık sorgusu hem RAM'i hem AsyncTCP bağlamını zorlar.
constexpr uint16_t MAX_PAGE = 240;

/// Bir geçmiş kaydı — SABİT boyutlu (değişken kayıt halka mantığını
/// karmaşıklaştırır).
///
/// Değerler `int16` ve ÖLÇEKLİ: `float` kaydı iki katı yer kaplardı ve bu
/// veri grafik çizmek için, hesap yapmak için değil.
struct Record
{
    uint32_t seq;            ///< 1'den başlar; 0 = geçersiz slot
    uint32_t epoch;          ///< duvar saati (2106'ya kadar)
    int16_t  values[SENSOR_SLOTS];
    uint8_t  qualityMask;    ///< bit i = sensör i ölçümü `OK` idi
    uint8_t  actuatorMask;   ///< bit i = aktüatör i açıktı
    uint8_t  flags;          ///< bit0 = timeValid
    uint8_t  crc8;
};

constexpr uint8_t FLAG_TIME_VALID = 0x01u;

// Kayıt boyutu SÖZLEŞMEDİR: dosya düzeni ona göre hesaplanıyor. Bir alan
// eklenirse derleme durur ve kapasite hesabı yeniden yapılmak zorunda kalır.
static_assert(sizeof(Record) == RECORD_BYTES, "Record 24 bayt olmali - kapasite hesabi buna dayali");
static_assert(FILE_BYTES == 491520u, "halka dosya 480 KB olmali");

/// Sensör başına ölçek çarpanı. `int16` aralığında kalır.
///   sıcaklık ×10 · pH ×100 · EC ×100 · debi ×100 · seviye ×1 · nem ×1
int16_t scaleFor(core::SensorId id, float v);
float   unscale(core::SensorId id, int16_t raw);

core::ErrCode begin();

/// Bir örnek ekler. **`store` task'ından çağrılır** — çağıran beklemez.
core::ErrCode append(const core::SystemState& snap, bool timeValid);

/// En yeni `count` kaydı okur (en eskiden yeniye sıralı).
///
/// @param out   en az `count` elemanlı dizi
/// @param count `MAX_PAGE`'i aşamaz
/// @return okunan kayıt sayısı
uint16_t readRecent(Record* out, uint16_t count);

/// Zaman aralığı sorgusu — `[fromEpoch, toEpoch]`, sayfalı.
uint16_t readRange(uint32_t fromEpoch, uint32_t toEpoch, uint16_t skip, Record* out,
                   uint16_t maxCount);

/// Şu ana kadar yazılan toplam kayıt (halka sarmış olabilir).
uint32_t totalWritten();

/// Dosyada bulunan geçerli kayıt sayısı.
uint32_t storedCount();

/// Bozuk (CRC hatalı) kayıt sayısı — teşhis.
uint32_t corruptCount();

/// Yazma hatası sayısı — sessizce yutulmaz.
uint32_t writeErrors();

} // namespace history
} // namespace services
