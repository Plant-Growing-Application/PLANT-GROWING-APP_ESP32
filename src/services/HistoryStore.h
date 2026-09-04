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

/// Halka dosyanın yolu. **Sürüm 2** (TASK-073).
///
/// Ad değişikliği bir GEÇERSİZ KILMA aracıdır. Sürüm 1 dosyası (`/hist.bin`)
/// iki nedenle okunamaz hâle geldi:
///
///   1. Kayıt boyutu 24 → 28 bayt (sensör slotu 6 → 8) — ofis hesabı kayar.
///   2. Daha önemlisi: **sürüm 1 verisi zaten bozuktu** (ISSUE-034). Yazıcı
///      slotları yayın sırasına, okuyucu farklı bir sabit sıraya göre
///      yorumluyordu; grafikte pH yerine su sıcaklığı, üstelik yanlış
///      ölçekle gösteriliyordu. O veriyi taşımanın bir anlamı yok.
///
/// Dosya başlığı eklemek yerine ad değiştirildi: halka ofset matematiği
/// dosyanın 0'dan başlamasına dayanıyor ve araya başlık koymak `offsetOf`,
/// `findValidCount` ve `findRotation`'ın tamamını etkilerdi. Ad değişimi
/// aynı işi tek satırda ve risksiz yapar.
constexpr const char* FILE_PATH = "/hist2.bin";

/// Boot'ta silinecek eski sürüm dosyası.
constexpr const char* LEGACY_FILE_PATH = "/hist.bin";

/// Kayıttaki sensör slotu sayısı — sistemdeki TÜM sensörler.
constexpr uint8_t  SENSOR_SLOTS  = 8;

/// Kapasite: 17408 × 28 = 476 KB. Sürüm 1'de 20480 × 24 = 480 KB idi.
///
/// Kayıt büyüdüğü hâlde DOSYA BOYUTU KORUNDU: LittleFS bölümü 896 KB ve
/// kalan alan aşınma dengeleme, meta veri ve web varlıkları için gerekli.
/// Dosyayı 560 KB'a çıkarmak o payı yerdi.
///
/// Bedeli açıkça kabul ediliyor: 60 sn periyotla 14,2 gün → **12,1 gün**.
constexpr uint32_t RECORD_COUNT  = 17408u;
constexpr uint32_t RECORD_BYTES  = 28u;
constexpr uint32_t FILE_BYTES    = RECORD_COUNT * RECORD_BYTES;   // 476 KB

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
    int16_t  values[SENSOR_SLOTS];   ///< sıra: `SLOT_ORDER`
    uint8_t  qualityMask;    ///< bit i = `SLOT_ORDER[i]` ölçümü `OK` idi
    uint8_t  actuatorMask;   ///< bit i = aktüatör i açıktı
    uint8_t  flags;          ///< bit0 = timeValid
    uint8_t  crc8;
};

// ---------------------------------------------------------------------------
// SLOT SIRASI — TEK DOĞRULUK KAYNAĞI (ISSUE-034)
//
// ── NE OLMUŞTU ──────────────────────────────────────────────────────────────
// `append()` slotları **yayın sırasına** (sensör kayıt tablosunun sırasına)
// göre dolduruyordu; `HistoryApi` ise kendi içinde ELLE YAZILMIŞ, farklı bir
// sırayı okuyordu. İki doğruluk kaynağı vardı ve sessizce ayrışmışlardı:
//
//     slot 0: yazılan seviye (0–2)  ·  okunan "su sıcaklığı" ÷10  → 0.2 °C
//     slot 2: yazılan su sıc. 19.4  ·  okunan "pH" ÷100           → 1.94 pH
//     slot 4: yazılan EC 1.35       ·  okunan "seviye" ×1         → 135
//
// Ölçek katsayıları da kimliğe bağlı olduğu için değerler yalnızca yanlış
// etiketlenmiyor, **yanlış hesaplanıyordu**.
//
// ── ÇÖZÜM ──────────────────────────────────────────────────────────────────
// Sıra ARTIK BURADA, tek bir tabloda. Yazıcı sensörü KİMLİĞİNE göre slotuna
// koyar (indeksine göre DEĞİL), okuyucu aynı tabloyu kullanır. İkisinin
// ayrışması imkânsız.
//
// Sıra `SensorId` enum sırasından bağımsızdır: geçmiş dosyası uzun ömürlüdür
// ve enum'a yeni bir sensör eklenmesi eski kayıtların anlamını
// DEĞİŞTİRMEMELİDİR. Yeni sensör tablonun SONUNA eklenir.
// ---------------------------------------------------------------------------
constexpr core::SensorId SLOT_ORDER[SENSOR_SLOTS] = {
    core::SensorId::WATER_TEMP,    // 0
    core::SensorId::WATER_FLOW,    // 1
    core::SensorId::PH,            // 2
    core::SensorId::EC,            // 3
    core::SensorId::WATER_LEVEL,   // 4
    core::SensorId::HUMIDITY,      // 5
    core::SensorId::AMBIENT_TEMP,  // 6  (TASK-066)
    core::SensorId::LIGHT,         // 7  (TASK-066)
};

/// Bir sensörün slot numarası; tabloda yoksa `SENSOR_SLOTS`.
constexpr uint8_t slotOf(core::SensorId id, uint8_t i = 0)
{
    return (i >= SENSOR_SLOTS) ? SENSOR_SLOTS
         : (SLOT_ORDER[i] == id) ? i
         : slotOf(id, static_cast<uint8_t>(i + 1u));
}

constexpr uint8_t FLAG_TIME_VALID = 0x01u;

// Kayıt boyutu SÖZLEŞMEDİR: dosya düzeni ona göre hesaplanıyor. Bir alan
// eklenirse derleme durur ve kapasite hesabı yeniden yapılmak zorunda kalır.
static_assert(sizeof(Record) == RECORD_BYTES,
              "Record 28 bayt olmali - kapasite hesabi ve dosya duzeni buna dayali");
static_assert(FILE_BYTES == 487424u, "halka dosya 476 KB olmali");

// Slot tablosu SENSOR_SLOTS ile aynı uzunlukta olmalı; eksik bir satır,
// yazılmayan bir sensör demektir.
static_assert(sizeof(SLOT_ORDER) / sizeof(SLOT_ORDER[0]) == SENSOR_SLOTS,
              "SLOT_ORDER uzunlugu SENSOR_SLOTS ile eslesmiyor");

// TEKRAR YOK. Aynı sensörün iki slotta olması, ikincisinin birinciyi
// ezmesi ve bir sensörün hiç kaydedilmemesi demektir — ve bu, ISSUE-034'ün
// tam olarak nasıl fark edilmeden yaşadığıdır.
constexpr bool slotsUnique(uint8_t i = 0, uint8_t j = 1)
{
    return (i >= SENSOR_SLOTS)      ? true
         : (j >= SENSOR_SLOTS)      ? slotsUnique(static_cast<uint8_t>(i + 1u),
                                                  static_cast<uint8_t>(i + 2u))
         : (SLOT_ORDER[i] == SLOT_ORDER[j]) ? false
         : slotsUnique(i, static_cast<uint8_t>(j + 1u));
}
static_assert(slotsUnique(), "SLOT_ORDER icinde ayni sensor iki kez var");

// Her slot GERÇEK bir sensör olmalı — `NONE` bir yer tutucu değildir.
constexpr bool slotsAreRealSensors(uint8_t i = 0)
{
    return (i >= SENSOR_SLOTS) ? true
         : (SLOT_ORDER[i] == core::SensorId::NONE ||
            static_cast<uint8_t>(SLOT_ORDER[i]) >= core::MAX_SENSORS) ? false
         : slotsAreRealSensors(static_cast<uint8_t>(i + 1u));
}
static_assert(slotsAreRealSensors(), "SLOT_ORDER gecersiz bir sensor kimligi tasiyor");

// Bit maskeleri 8 bitlik: slot sayısı 8'i aşarsa `qualityMask` sessizce
// üst slotları kaybederdi.
static_assert(SENSOR_SLOTS <= 8, "qualityMask 8 bit - SENSOR_SLOTS 8'i asamaz");

// Kaydedilmeyen sensör kalmamalı: sistemdeki her sensörün bir slotu var mı?
static_assert(SENSOR_SLOTS == core::MAX_SENSORS,
              "her sensorun bir gecmis slotu olmali");

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
