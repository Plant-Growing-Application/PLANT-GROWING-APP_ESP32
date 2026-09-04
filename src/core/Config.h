#pragma once

// Konfigürasyon şeması — TASK-014
//
// TEMEL KURAL: her alanın **varsayılanı ve geçerli aralığı** burada tanımlıdır.
// Geçersiz değer varsayılana düşer ve LOGLANIR. Mevcut sistemdeki sessiz
// `memset(&Setting, 0, ...)` davranışı yasaktır — sıfırlanmış bir güvenlik
// eşiği, kapalı bir korumadır (ARCHITECTURE §16.4).
//
// ŞİFRE BURADA YOK: yalnızca SSID taşınır. Şifre `SecretStore`'dadır
// (TASK-013). Bu ayrım, config'in tamamını serileştiren bir API'nin
// (TASK-044) şifreyi kazara sızdırmasını yapısal olarak imkânsız kılar.
//
// GÜVENLİ VARSAYILANLAR: cihaz ilk açıldığında **kendiliğinden sulamaz**.
// `automation.mode = MANUAL`, yardımcı aktüatörler kapalı, `maxRunMs` kısa.
// Bir varsayılan belirsizse güvenli olan seçilir.

#include <stdint.h>
#include <type_traits>

#include "CropProfile.h"
#include "Rule.h"
#include "SystemState.h"
#include "Types.h"

namespace core {

/// Şema sürümü.
///
/// **2 — otomasyon kuralları kalıcılaştı (ISSUE-021).** TASK-054 kural
/// modelini `Config`'e eklemişti ama NVS'te bir `rules` bölümü YOKTU: kural
/// kümesi yalnızca RAM'de yaşıyor ve her boot'ta boşalıyordu. Sürüm 2 ile
/// `cfg.rules` bölümü eklendi ve TASK-015'in migration yolu devreye girdi
/// (ARCHITECTURE §15.3): sürüm 1 kaydı okunduğunda kural kümesi BOŞ
/// varsayılanda kalır — sürüm 1'de zaten kural saklanamıyordu.
/// **3 — ürün profili seçimi kalıcılaştı (TASK-067).** `CropConfig` bölümü
/// eklendi. Sürüm 2 kaydı okunduğunda bu bölüm varsayılanda kalır:
/// `crop = NONE`, yani "ürün seçilmemiş" — cihazın TASK-067 öncesindeki
/// davranışıyla birebir aynı. Göçün taşıyacağı veri yoktur, çünkü sürüm 2'de
/// saklanabilecek bir ürün seçimi zaten yoktu.
constexpr uint16_t CONFIG_SCHEMA_VERSION = 3;

/// Kayıt geçerliliği işareti.
constexpr uint32_t CONFIG_MAGIC = 0x43464731u;  // "CFG1"

// ---------------------------------------------------------------------------
// network
// ---------------------------------------------------------------------------

/// IP yapılandırma modu.
///
/// AP/STA modundan **tamamen bağımsızdır**. Mevcut sistemde `_useDHCP`
/// yanlışlıkla `IsServerMode`'dan türetiliyordu (REQUIREMENTS §2) — iki
/// ilgisiz kavram birbirine bağlanmıştı.
enum class IpMode : uint8_t
{
    DHCP   = 0,
    STATIC = 1,
};

struct NetworkConfig
{
    FixedString<32> ssid;      ///< şifre burada DEĞİL (SecretStore)
    uint32_t        staticIp;  ///< ham IPv4; IpMode::STATIC iken kullanılır
    uint32_t        gateway;
    uint32_t        subnet;
    uint32_t        dns;
    IpMode          ipMode;
    uint8_t         reserved[3];
};

// ---------------------------------------------------------------------------
// sensors — kalibrasyon ve doğrulama parametreleri
// ---------------------------------------------------------------------------

struct SensorConfig
{
    Range<float> validRange;      ///< dışına çıkan değer OUT_OF_RANGE
    float        offset;          ///< kalibrasyon: değer = ham × scale + offset
    float        scale;
    float        maxChangePerSec; ///< fiziksel olmayan sıçrama sınırı (0 = kapalı)
    uint8_t      enabled;         ///< 0 = takılı değil → NOT_PRESENT
    uint8_t      filterStrength;  ///< 0 = filtresiz (güvenlik sensörü için)
    uint8_t      reserved[2];
};

// ---------------------------------------------------------------------------
// actuators — çalışma kısıtları
//
// Bu kısıtlar `ActuatorManager` (TASK-029) tarafından uygulanır. Otomasyon
// motoru bunları BİLMEZ (ARCHITECTURE §11.4).
// ---------------------------------------------------------------------------

struct ActuatorConfig
{
    uint32_t minRunMs;    ///< açıldıktan sonra en az bu kadar çalışsın
    uint32_t maxRunMs;    ///< bundan uzun çalışmasın — ÜST SINIRI VAR
    uint32_t cooldownMs;  ///< kapandıktan sonra bu kadar beklesin
    uint8_t  relayIndex;  ///< fiziksel röle eşlemesi (mantıksal/fiziksel ayrım)
    //  NOT: `activeLow` BURADA DEGIL — derleme zamani sabiti (BoardPins.h).
    //  Boot Asama 1 rolelerı config yuklenmeden ONCE guvenli seviyeye alir;
    //  polarite o anda bilinmek zorunda (TASK-017 Karar 1).
    uint8_t  enabled;
    uint8_t  reserved;
};

// ---------------------------------------------------------------------------
// safety — güvenlik eşikleri
//
// Bu bölümdeki her alan doğrudan donanım güvenliğini belirler. Doğrulaması
// zorunludur ve API'den gevşetilemez.
// ---------------------------------------------------------------------------

struct SafetyConfig
{
    uint32_t flowVerifyDelayMs;  ///< pompa açıldıktan sonra akış kontrolü gecikmesi
    float    flowMinRate;        ///< bu debinin altı = kuru çalışma (L/dk)
    uint32_t maxRuntimeGraceMs;  ///< maxRunMs aşımında tanınan pay
    uint8_t  maxRuntimeViolations; ///< bu kadar tekrarda acil duruma geç
    uint8_t  requireLevelSensor; ///< 1 = seviye sensörü yoksa pompa kilitli
    uint8_t  reserved[2];
};

// ---------------------------------------------------------------------------
// automation
//
// Kural yapısı `Rule.h` içinde (TASK-054); kural kümesi `Config::rules`
// alanında taşınır ve şema sürümü 2'den itibaren NVS'te ayrı bir bölüm
// olarak saklanır (ISSUE-021).
// ---------------------------------------------------------------------------

struct AutomationConfig
{
    uint32_t       manualOverrideMs;  ///< AUTO modda manuel komutun geçerlilik süresi
    AutomationMode mode;              ///< varsayılan MANUAL — kendiliğinden sulamaz
    uint8_t        reserved[3];
};

// ---------------------------------------------------------------------------
// crop — hangi ürün, hangi dönem (TASK-067)
//
// PARAMETRELER BURADA DEĞİL: profil tabloları `.rodata`'dadır
// (`core/CropProfile.h`). Burada yalnızca SEÇİM saklanır. Bu ayrım sayesinde
// altı ürünlük katalog NVS blob'una hiç dokunmaz ve profil tanımları firmware
// ile birlikte sürümlenir — kullanıcının NVS'inde bozulmuş bir çilek profili
// olamaz.
// ---------------------------------------------------------------------------

struct CropConfig
{
    /// Dikim tarihi (Unix epoch, saniye). 0 = bilinmiyor.
    ///
    /// `int64`: `core::EpochSeconds` ile aynı genişlik. 2038 sorunundan
    /// etkilenmez ve duvar saati tipiyle sessizce daralan bir dönüşüm olmaz.
    int64_t plantedAtEpoch;

    CropId      crop;
    GrowthStage stage;

    /// 1 = dönem, dikimden bu yana geçen güne göre KENDİLİĞİNDEN ilerler.
    ///
    /// Zaman geçersizken ilerleme DURUR (donanımsal RTC yok — ISSUE-005).
    /// Geçersiz saatle gün saymak, güç kesintisi sonrası meyve dönemindeki
    /// çileği fide dönemine geri döndürür ve EC hedefini yarıya indirirdi.
    uint8_t autoStage;

    Intensity intensity;

    /// `crop == CUSTOM` iken: kurallar HANGİ profilden türetilmişti.
    ///
    /// Arayüz "Çilekten türetildi (elle değiştirildi)" diyebilsin diye
    /// saklanır. `CropId::NONE` = bilinmiyor.
    CropId derivedFrom;

    uint8_t reserved[2];
};

// ---------------------------------------------------------------------------
// system
// ---------------------------------------------------------------------------

struct SystemConfig
{
    /// POSIX TZ dizesi — yaz saati kuralları dahil.
    /// Mevcut sistemde sabit `GMT+3` kodlanmıştı ve DST desteği yoktu.
    FixedString<40> timezone;
    uint16_t        telemetryIntervalMs;  ///< WS durum yayını hız sınırı
    uint8_t         logLevel;             ///< seri port için en düşük LogLevel
    uint8_t         reserved;
};

// ---------------------------------------------------------------------------
// Kök yapı
// ---------------------------------------------------------------------------

struct Config
{
    uint32_t magic;
    uint16_t schemaVersion;
    uint16_t reserved;

    /// **Başta duruyor, bilinçli olarak.** `CropConfig` `int64` taşır ve 8
    /// bayta hizalanır; yapının ortasına konsaydı derleyici öncesine 4 bayt
    /// dolgu koyardı. Başlık zaten 8 baytlık olduğu için burada dolgu sıfırdır.
    CropConfig       crop;

    NetworkConfig    network;
    SensorConfig     sensors[MAX_SENSORS];
    ActuatorConfig   actuators[MAX_ACTUATORS];
    SafetyConfig     safety;
    AutomationConfig automation;

    /// Otomasyon kuralları (TASK-054). Varsayılan: TAMAMEN BOŞ —
    /// ilk açılışta sistem kendiliğinden sulamaya başlamaz.
    RuleSet          rules;
    SystemConfig     system;
};

// ---------------------------------------------------------------------------
// Geçerli aralıklar — doğrulamanın tek doğruluk kaynağı
//
// TASK-044 (API) bu sınırları YENİDEN TANIMLAMAZ; aynı doğrulama
// fonksiyonlarını çağırır.
// ---------------------------------------------------------------------------
namespace limits {

/// `maxRunMs`'in ÜST SINIRI — pompalar. Sınırsız bırakılırsa "maksimum
/// çalışma süresi" koruması anlamsızlaşır.
constexpr Range<uint32_t> ACTUATOR_MAX_RUN{1000u, 2u * 60u * 60u * 1000u};   // 1 sn – 2 saat
constexpr Range<uint32_t> ACTUATOR_MIN_RUN{0u, 10u * 60u * 1000u};           // 0 – 10 dk
constexpr Range<uint32_t> ACTUATOR_COOLDOWN{0u, 60u * 60u * 1000u};          // 0 – 1 saat

// ── AKTÜATÖR BAŞINA `maxRunMs` ÜST SINIRI (TASK-066) ────────────────────────
//
// TEK BİR GLOBAL SINIR ARTIK YETMİYOR. Büyütme ışığı günde 14–18 saat yanmak
// zorundadır; 2 saatlik pompa sınırı ona uygulansaydı `ActuatorManager` ışığı
// her 2 saatte bir zorla kapatır ve kullanıcı "ışık sönüyor" derdinin
// kaynağını asla bulamazdı. Sınırı GLOBAL olarak 18 saate çıkarmak ise pompa
// korumasını yok ederdi — 18 saat kuru çalışan bir pompa yanar.
//
// Bu yüzden sınır role göre ayrıldı; gevşetme yalnızca gevşetilmesi GEREKEN
// aktüatörde geçerli.
constexpr Range<uint32_t> ACTUATOR_MAX_RUN_LIGHT{1000u, 20u * 60u * 60u * 1000u};  // ≤ 20 sa
constexpr Range<uint32_t> ACTUATOR_MAX_RUN_HEATER{1000u, 6u * 60u * 60u * 1000u};  // ≤ 6 sa

/// Dozaj pompası: SIKI sınır. Takılı kalan bir dozaj pompası besin bidonunun
/// tamamını hazneye boşaltır — bu, bitkiyi bir pompa arızasından daha hızlı
/// öldürür. Saniyeler mertebesinde çalışması beklenir.
constexpr Range<uint32_t> ACTUATOR_MAX_RUN_DOSING{1000u, 5u * 60u * 1000u};        // ≤ 5 dk

/// Aktüatörün rolüne göre geçerli `maxRunMs` aralığı.
///
/// `ActuatorId` yerine indeks alır: `validateActuator()` dizinin indeksiyle
/// çağrılır ve o indeksin kimliğe karşılık geldiği `SystemState.h`'ta
/// `static_assert` ile zaten kilitlidir.
constexpr Range<uint32_t> maxRunLimitFor(uint8_t index)
{
    return (index == static_cast<uint8_t>(ActuatorId::GROW_LIGHT))    ? ACTUATOR_MAX_RUN_LIGHT
         : (index == static_cast<uint8_t>(ActuatorId::HEATER))        ? ACTUATOR_MAX_RUN_HEATER
         : (index == static_cast<uint8_t>(ActuatorId::NUTRIENT_PUMP)) ? ACTUATOR_MAX_RUN_DOSING
                                                                      : ACTUATOR_MAX_RUN;
}

constexpr Range<uint32_t> FLOW_VERIFY_DELAY{1000u, 60u * 1000u};             // 1 – 60 sn
constexpr Range<float>    FLOW_MIN_RATE{0.01f, 1000.0f};                     // L/dk
constexpr Range<uint32_t> MAX_RUNTIME_GRACE{0u, 60u * 1000u};                // 0 – 60 sn

constexpr Range<uint32_t> MANUAL_OVERRIDE{60u * 1000u, 24u * 60u * 60u * 1000u};  // 1 dk – 24 sa
constexpr Range<uint16_t> TELEMETRY_INTERVAL{200u, 60000u};                  // 200 ms – 60 sn
constexpr Range<uint8_t>  FILTER_STRENGTH{0u, 32u};

} // namespace limits

/// Güvenli varsayılanları yükler.
void loadDefaults(Config& out);

// ---------------------------------------------------------------------------
// Derleme zamanı doğrulama
// ---------------------------------------------------------------------------

static_assert(std::is_trivially_copyable<Config>::value,
              "Config trivially copyable olmali (NVS blob olarak yazilacak)");
static_assert(std::is_standard_layout<Config>::value, "Config standard layout olmali");
// ── BOYUT NÖBETÇİSİ ─────────────────────────────────────────────────────────
//
// ÖLÇÜLEN (TASK-072, firmware.elf sembol tablosundan): **624 bayt**.
//   header 8 · crop 16 · network 56 · sensors 8×24 · actuators 5×16
//   safety 16 · automation 8 · rules 196 · system 46 (+ hizalama)
//
// Sınır KEYFİDİR, NVS'in teknik sınırı değil: bölümler ayrı anahtarlarda
// saklanır ve en büyüğü `rules` (196 bayt) — NVS blob sınırı bunun onlarca
// katı. Buradaki sayının işi, yapının FARK EDİLMEDEN büyümesini engellemek.
//
// 640'tan 704'e çıkarıldı: eski değerde yalnızca 16 bayt pay kalmıştı ve
// tek bir `uint32_t` alan ekleyen kişi, gerçek bir sorun olmadığı hâlde
// derleme hatasıyla karşılaşıp sınırı düşünmeden yükseltirdi. Nöbetçinin
// işe yaraması için payın anlamlı olması gerekir.
static_assert(sizeof(Config) <= 704,
              "Config 704 bayti asti — buyume kasitli mi? olculen deger 624 idi");

// Aralık tanımlarının kendisi tutarlı mı?
static_assert(limits::ACTUATOR_MAX_RUN.valid(), "ACTUATOR_MAX_RUN araligi tutarsiz");
static_assert(limits::ACTUATOR_MAX_RUN_LIGHT.valid(), "ACTUATOR_MAX_RUN_LIGHT tutarsiz");
static_assert(limits::ACTUATOR_MAX_RUN_HEATER.valid(), "ACTUATOR_MAX_RUN_HEATER tutarsiz");
static_assert(limits::ACTUATOR_MAX_RUN_DOSING.valid(), "ACTUATOR_MAX_RUN_DOSING tutarsiz");
static_assert(limits::FLOW_VERIFY_DELAY.valid(), "FLOW_VERIFY_DELAY araligi tutarsiz");
static_assert(limits::ACTUATOR_MAX_RUN.max > limits::ACTUATOR_MIN_RUN.min,
              "maxRun ust siniri minRun alt sinirindan buyuk olmali");

// Işık sınırı bir günü AŞMAMALI: 24 saati geçen bir "maksimum çalışma süresi"
// hiç tetiklenmez ve koruma sessizce yok olur.
static_assert(limits::ACTUATOR_MAX_RUN_LIGHT.max < 24u * 60u * 60u * 1000u,
              "isik maxRun siniri bir gunu asamaz — koruma anlamsizlasir");

// Dozaj sınırı pompa sınırından GEVŞEK OLAMAZ; gevşerse ayrı tanımlamanın
// amacı (aşırı gübreleme koruması) tersine döner.
static_assert(limits::ACTUATOR_MAX_RUN_DOSING.max < limits::ACTUATOR_MAX_RUN.max,
              "dozaj siniri pompa sinirindan siki olmali");

// Rol tablosu enum ile hizalı mı? Yanlış indeks, ışığa dozaj sınırı verirdi.
static_assert(limits::maxRunLimitFor(static_cast<uint8_t>(ActuatorId::WATER_PUMP)).max ==
                  limits::ACTUATOR_MAX_RUN.max,
              "su pompasi pompa sinirini kullanmali");
static_assert(limits::maxRunLimitFor(static_cast<uint8_t>(ActuatorId::GROW_LIGHT)).max ==
                  limits::ACTUATOR_MAX_RUN_LIGHT.max,
              "isik gevsetilmis siniri kullanmali");
static_assert(limits::maxRunLimitFor(static_cast<uint8_t>(ActuatorId::NUTRIENT_PUMP)).max ==
                  limits::ACTUATOR_MAX_RUN_DOSING.max,
              "dozaj pompasi siki siniri kullanmali");

} // namespace core
