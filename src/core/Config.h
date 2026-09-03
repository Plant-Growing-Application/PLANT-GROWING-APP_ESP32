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
constexpr uint16_t CONFIG_SCHEMA_VERSION = 2;

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

/// `maxRunMs`'in ÜST SINIRI. Sınırsız bırakılırsa "maksimum çalışma süresi"
/// koruması anlamsızlaşır.
constexpr Range<uint32_t> ACTUATOR_MAX_RUN{1000u, 2u * 60u * 60u * 1000u};   // 1 sn – 2 saat
constexpr Range<uint32_t> ACTUATOR_MIN_RUN{0u, 10u * 60u * 1000u};           // 0 – 10 dk
constexpr Range<uint32_t> ACTUATOR_COOLDOWN{0u, 60u * 60u * 1000u};          // 0 – 1 saat

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
static_assert(sizeof(Config) <= 640, "Config 640 bayti asmamali (NVS blob)");

// Aralık tanımlarının kendisi tutarlı mı?
static_assert(limits::ACTUATOR_MAX_RUN.valid(), "ACTUATOR_MAX_RUN araligi tutarsiz");
static_assert(limits::FLOW_VERIFY_DELAY.valid(), "FLOW_VERIFY_DELAY araligi tutarsiz");
static_assert(limits::ACTUATOR_MAX_RUN.max > limits::ACTUATOR_MIN_RUN.min,
              "maxRun ust siniri minRun alt sinirindan buyuk olmali");

} // namespace core
