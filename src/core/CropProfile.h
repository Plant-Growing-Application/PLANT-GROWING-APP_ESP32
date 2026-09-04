#pragma once

// Ürün (bitki) profilleri — TASK-067
//
// ── PROFİL VERİDİR, MOTOR KODDUR ────────────────────────────────────────────
// Bu dosya `Rule.h`'ın bir katman üstüdür. `Rule.h` "şu aktüatör açık olsun"
// diyebilen bir dil tanımlar; burası o dilde YAZILMIŞ, ürüne özgü bir metindir.
// Kural motoru ürünleri BİLMEZ ve bilmemelidir.
//
// Sonuç: yeni bir ürün eklemek bu dosyadaki tabloya bir satırdır — otomasyon
// motoruna, güvenlik zincirine veya aktüatör kısıtlarına dokunulmaz.
//
// ── NEDEN FLASH'TA, CONFIG'TE DEĞİL ─────────────────────────────────────────
// `Config` bir NVS blob'udur ve boyutu `static_assert` ile sınırlıdır. Altı
// ürün × dört dönem × beş hedef aralığı ~1,3 KB tutar; bunu config'e koymak
// blob sınırını kat kat aşardı. Profiller `.rodata`'da SABİTTİR; config'te
// yalnızca "hangi ürün, hangi dönem" (`CropConfig`) saklanır.
//
// Yan fayda: profil tanımı firmware ile birlikte sürümlenir. Kullanıcının
// NVS'inde bozulmuş bir çilek profili OLAMAZ.
//
// ── DEĞERLER NEREDEN GELİYOR ────────────────────────────────────────────────
// Aşağıdaki pH/EC/sıcaklık aralıkları hidroponik kaynaklarının ortak
// aralığıdır ve `docs/CROP_PROFILES.md`'de kaynaklarıyla listelenmiştir.
// BUNLAR BAŞLANGIÇ DEĞERİDİR: kaynaklar arasında %20-30 sapma olağandır ve
// kullanıcı üretilen kuralların hepsini elle değiştirebilir.
//
// ── core/ İÇİNDE OLMASININ GEREKÇESİ ────────────────────────────────────────
// `CropConfig` `Config`'in parçasıdır ve `Config` `core/`'dadır (D5: core/
// hiçbir katmana bağımlı değildir). Kural ÜRETİMİ de burada: saf bir
// dönüşümdür, donanım görmez ve `pio test -e native` ile PC'de koşar.

#include <stdint.h>

#include "Rule.h"
#include "SystemState.h"
#include "Types.h"

namespace core {

/// Katalogdaki ürün sayısı.
constexpr uint8_t CROP_CATALOG_COUNT = 6;

/// Bir profilin taşıyabileceği en fazla dönem sayısı.
///
/// Yapraklı ürünlerde (marul, fesleğen) çiçeklenme ve meyve dönemi YOKTUR;
/// onlar iki dönem kullanır. Bu yüzden dizi boyutu sabit, GEÇERLİ dönem
/// sayısı profil başına değişkendir (`CropProfile::stageCount`).
constexpr uint8_t CROP_MAX_STAGES = 4;

/// Ürün kimliği. Değerler KARARLIDIR: config'te saklanır ve API'de taşınır.
enum class CropId : uint8_t
{
    NONE       = 0,  ///< ürün seçilmemiş — sistemin kutudan çıktığı hâli
    STRAWBERRY = 1,
    TOMATO     = 2,
    PEPPER     = 3,
    CUCUMBER   = 4,
    LETTUCE    = 5,
    BASIL      = 6,

    /// Bir profil uygulandıktan SONRA kullanıcı kuralları elle değiştirdi.
    ///
    /// Ayrı bir kimlik olması bilinçlidir: arayüz "Çilek profili aktif"
    /// demeye devam etseydi YALAN SÖYLERDİ — ekranda çilek yazarken kurallar
    /// artık çilek profilinin ürettiği kurallar değildir.
    CUSTOM     = 0xFF,
};

/// Gelişim dönemi. Sayısal değer `CropProfile::stages[]` indeksidir.
enum class GrowthStage : uint8_t
{
    SEEDLING   = 0,  ///< fide
    VEGETATIVE = 1,  ///< gelişme (yaprak/kök)
    FLOWERING  = 2,  ///< çiçeklenme
    FRUITING   = 3,  ///< meyve
};

/// Sulama yoğunluğu — kullanıcının tek anlaşılır ayarı.
///
/// Sistem tipi (NFT, damlama, DWC), bitki sayısı ve ortam sıcaklığı sulama
/// ihtiyacını değiştirir ama cihaz bunların hiçbirini ölçemez. Kullanıcıya on
/// parametre sormak yerine TEK bir kaydırıcı veriyoruz; profilin çevrim
/// süresini ölçekler.
/// ── MAKRO ÇAKIŞMASI (ISSUE-009 ile aynı sınıf hata) ─────────────────────────
/// İlk yazımda değerler `LOW` / `NORMAL` / `HIGH` idi ve derleme kırıldı:
/// `LOW` ve `HIGH`, `esp32-hal-gpio.h` içinde `#define LOW 0x0` olarak
/// tanımlıdır. Önişlemci kapsam tanımaz — `Intensity::LOW` metin olarak
/// `Intensity::0x0`'a dönüşür. `enum class` bu tuzağa karşı koruma SAĞLAMAZ.
///
/// Değerler bu yüzden Arduino sözlüğüyle çakışmayan adlar taşıyor.
enum class Intensity : uint8_t
{
    SPARSE   = 0,  ///< az sula
    NORMAL   = 1,
    ABUNDANT = 2,  ///< bol sula
};

/// Bir dönemin hedefleri ve çalışma parametreleri.
///
/// **Hedef aralıklar ile üretilen kurallar farklı şeylerdir.** Aralıklar
/// arayüzde "iyi / dikkat" bandı olarak GÖSTERİLİR; yalnızca bir kısmı kurala
/// dönüşür — sistemde pH'ı değiştirecek bir aktüatör yoktur, bu yüzden pH
/// bandı tavsiyeden ibarettir.
struct CropStage
{
    Range<float> ph;         ///< hedef pH bandı — GÖSTERİM (pH dozaj donanımı yok)
    Range<float> ec;         ///< hedef EC bandı (mS/cm) — besin kuralını besler
    Range<float> waterTemp;  ///< besin çözeltisi sıcaklığı — ısıtıcı kuralını besler
    Range<float> airTemp;    ///< ortam sıcaklığı — GÖSTERİM
    Range<float> humidity;   ///< bağıl nem (%) — GÖSTERİM

    uint16_t lightMinutesPerDay;  ///< fotoperiyot — ışık penceresi kuralını besler
    uint16_t irrigationOnS;       ///< sulama çevrimi: açık kalma süresi
    uint16_t irrigationPeriodS;   ///< sulama çevrimi: toplam periyot
    uint16_t aerationOnS;         ///< havalandırma çevrimi: açık kalma süresi
    uint16_t aerationPeriodS;     ///< havalandırma çevrimi: toplam periyot

    /// Bu dönemin tipik uzunluğu (gün). **0 = son dönem**, süresiz.
    /// Otomatik dönem ilerlemesi bu değerleri toplayarak karar verir.
    uint16_t durationDays;
};

/// Bir ürün profili.
struct CropProfile
{
    CropId  id;
    uint8_t stageCount;  ///< geçerli dönem sayısı (yapraklılarda 2)
    uint8_t difficulty;  ///< 1 = kolay · 2 = orta · 3 = zor (arayüzde rozet)
    uint8_t reserved;

    const char* key;   ///< API/JSON adı — "strawberry"
    const char* name;  ///< arayüz adı — "Çilek"

    CropStage stages[CROP_MAX_STAGES];
};

// ---------------------------------------------------------------------------
// Katalog erişimi
// ---------------------------------------------------------------------------

/// Katalogdaki ürün sayısı.
uint8_t cropCount();

/// Sıra numarasına göre profil. `index >= cropCount()` ise `nullptr`.
const CropProfile* cropAt(uint8_t index);

/// Kimliğe göre profil. `NONE`, `CUSTOM` ve tanınmayan kimlikler `nullptr`.
const CropProfile* cropById(CropId id);

/// API adına göre profil ("strawberry"). Bulunamazsa `nullptr`.
const CropProfile* cropByKey(const char* key);

/// Kimlik → API adı. Katalog dışı kimlikler için "none" / "custom".
const char* cropKeyOf(CropId id);

/// API adı → kimlik. "none" ve "custom" da tanınır.
bool cropIdFromKey(const char* key, CropId& out);

/// Dönem → API adı ("seedling", "vegetative", "flowering", "fruiting").
const char* stageKeyOf(GrowthStage s);

/// API adı → dönem.
bool stageFromKey(const char* key, GrowthStage& out);

/// Bu dönem profilde geçerli mi? (Yapraklı üründe `FRUITING` geçersizdir.)
bool stageValidFor(const CropProfile& p, GrowthStage s);

// ---------------------------------------------------------------------------
// Kural üretimi
// ---------------------------------------------------------------------------

/// Işık penceresinin başlangıcı — gün içi dakika (06:00).
///
/// Sabit: "ne zaman ışık versin" sorusu kullanıcıya sorulacak kadar önemli
/// değil, ama sabahı seçmek gerçek gün ışığıyla örtüşür ve seranın gece
/// soğumasına izin verir. Kullanıcı üretilen kuralı elle kaydırabilir.
constexpr uint16_t LIGHT_WINDOW_START_MIN = 6u * 60u;

/// Isıtıcı histerezis bandı (°C).
///
/// Sıfır bant, sıcaklık eşiğin bir kılpayı altında/üstünde gezinirken
/// ısıtıcının saniyede birkaç kez açılıp kapanması demektir; röle kontakları
/// bu şekilde haftalar içinde yapışır.
constexpr float HEATER_HYSTERESIS_C = 1.5f;

/// Besin dozajı histerezis bandı (mS/cm).
///
/// EC hedefin ALTINA düşünce dozaj başlar, `ec.min + bu bant`'a ulaşınca
/// durur. Bant dar tutulur: dozajdan sonra karışma zaman aldığı için asıl
/// aşırı-gübreleme koruması `ActuatorConfig::cooldownMs`'tir, bu bant değil.
constexpr float EC_DOSE_BAND = 0.2f;

/// Üretilen bir sulama çevriminin açık kalabileceği en uzun süre (saniye).
///
/// Su pompasının varsayılan `maxRunMs`'i 5 dakikadır. Profilin bundan uzun
/// bir çevrim üretmesi, `ActuatorManager`'ın pompayı her çevrimde SÜRE AŞIMI
/// gerekçesiyle zorla kapatması demek olurdu — kullanıcı kural ekranında
/// doğru görünen bir çevrimin neden kesildiğini asla bulamazdı.
constexpr uint16_t MAX_GENERATED_ON_S = 240u;

/// Profil + dönem + yoğunluktan kural kümesi üretir.
///
/// ── ÜRETİLEN KURALLAR ETKİN, MOTOR KAPALI ───────────────────────────────────
/// Kurallar `enabled = 1` doğar; amaçları budur. Sistemin kendiliğinden
/// sulamaya başlamamasını sağlayan şey `automation.mode`'un `MANUAL` kalması
/// ve bu fonksiyonun ona DOKUNMAMASIDIR. Kural kümesi hazır ve görünür durur;
/// motor, operatör `AUTO`'ya alana kadar hiçbirini değerlendirmez
/// (ARCHITECTURE §11.1). M4 güvenlik kapısı bu yüzden kapalı kalır.
///
/// ── ÇALIŞAMAYACAK KURAL ÜRETİLMEZ ───────────────────────────────────────────
/// `actuatorEnabled` kullanıcının "bu röle fiilen kablolu" beyanı,
/// `sensorEnabled` ise "bu sensör takılı" beyanıdır. İkisi de gereklidir:
///
///   · ısıtıcı rölesi yoksa   → ısıtma kuralı hiçbir şeyi sürmez
///   · sıcaklık sensörü yoksa → kural HİÇ tetiklenmez, sessizce ölü durur
///
/// İkinci durum daha sinsidir: kural listesinde doğru görünen bir satır,
/// ölçüm olmadığı için asla değerlendirilmez ve kullanıcı nedenini kural
/// ekranında arar. Üretmemek, üretip açıklamaktan iyidir.
///
/// @param p               ürün profili
/// @param stage           uygulanacak dönem (profilde geçerli olmalı)
/// @param intensity       sulama yoğunluğu
/// @param actuatorEnabled `MAX_ACTUATORS` uzunluğunda; hangi röleler bağlı
/// @param sensorEnabled   `MAX_SENSORS` uzunluğunda; hangi sensörler takılı
/// @param out             üretilen küme — **tamamen yeniden yazılır**
/// @return üretilen kural sayısı
uint8_t buildCropRules(const CropProfile& p, GrowthStage stage, Intensity intensity,
                       const bool* actuatorEnabled, const bool* sensorEnabled, RuleSet& out);

/// Dikimden bu yana geçen güne göre dönem hesaplar.
///
/// Zaman GEÇERSİZSE bu fonksiyon çağrılmamalıdır: çağıran `TimeService`'e
/// sorar ve geçersizse dönemi DONDURUR. Geçersiz saatle gün saymak, güç
/// kesintisi sonrası çileği fideye geri döndürürdü.
///
/// @param p                 profil
/// @param daysSincePlanting dikimden bu yana geçen tam gün
/// @return o güne düşen dönem; süre tabloyu aşarsa SON dönem
GrowthStage stageForDay(const CropProfile& p, uint32_t daysSincePlanting);

// --- Derleme zamanı doğrulama ----------------------------------------------

static_assert(CROP_MAX_STAGES == 4,
              "donem sayisi degistiyse CropStage tablolari gozden gecirilmeli");
static_assert(static_cast<uint8_t>(GrowthStage::FRUITING) == CROP_MAX_STAGES - 1,
              "GrowthStage degerleri stages[] indeksi olmali");
static_assert(MAX_GENERATED_ON_S < 300u,
              "uretilen cevrim su pompasinin varsayilan maxRunMs'ini asmamali");
static_assert(LIGHT_WINDOW_START_MIN < 1440u, "isik penceresi baslangici gun ici olmali");

} // namespace core
