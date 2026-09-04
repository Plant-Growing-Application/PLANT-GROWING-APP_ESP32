#include "ConfigService.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string.h>

#include "core/Diagnostics.h"
#include "hal/NvsStore.h"
#include "hal/SecretStore.h"

namespace services {
namespace config {
namespace {

using core::ErrCode;

// Bölüm anahtarları — hepsi NVS'in 15 karakter sınırının altında.
constexpr const char* KEY_VERSION = "ver";
constexpr const char* KEY_NET     = "net";
constexpr const char* KEY_SENS    = "sens";
constexpr const char* KEY_ACT     = "act";
constexpr const char* KEY_SAFE    = "safe";
constexpr const char* KEY_AUTO    = "auto";
constexpr const char* KEY_RULES   = "rules";
constexpr const char* KEY_SYS     = "sys";
constexpr const char* KEY_CROP    = "crop";

/// `rules` bölümünün ilk göründüğü şema sürümü. Daha eski bir kayıtta bu
/// anahtar YOKTUR ve aranması gereksiz bir CFG_NOT_FOUND uyarısı üretir.
constexpr uint16_t RULES_SINCE_VERSION = 2;

/// `crop` bölümünün ilk göründüğü şema sürümü (TASK-067). Aynı gerekçe.
constexpr uint16_t CROP_SINCE_VERSION = 3;

core::Config     g_config{};
ConfigLoadResult g_result{};
uint8_t          g_dirtyMask = 0;

/// Yapı güncellemelerini yırtılmaya karşı koruyan spinlock (TASK-072).
///
/// YALNIZCA `rules` ve `actuators[]` için gerekli: bunlar tek bir okuyucu
/// döngüsü boyunca alan alan okunan YAPILARDIR. Skaler alanlar (eşikler, mod,
/// süreler) hizalı 1/2/4 bayt olduğu için Xtensa'da zaten atomiktir ve kilit
/// gerektirmez — hepsini korumak, kritik bölümü gereksiz yere uzatırdı.
///
/// Tutuluş süresi tek bir `memcpy` kadardır (196 bayt ≈ 0,5 µs @240 MHz).
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

/// Kural kümesi her değiştiğinde artar. Otomasyon motoru bunu okuyarak kural
/// çalışma durumlarını (histerezis, son tetikleme) sıfırlar: bir slotun
/// anlamı değiştiğinde eski çalışma durumunu devralmak, yeni kuralın yanlış
/// tarafından başlaması demektir.
///
/// Sayaç TEK YAZARLIDIR (bu modül) ve motor yalnızca okur; kilit gerekmez.
uint32_t g_rulesRevision = 0;

/// Sensör yapılandırması her değiştiğinde artar (ISSUE-035).
/// `SensorService` bunu izleyerek yeni etkinleştirilen sensörün sürücüsünü
/// çalışma anında başlatır — yeniden başlatma gerekmez.
uint32_t g_sensorsRevision = 0;

enum DirtyBit : uint8_t
{
    DIRTY_NET   = 1u << 0,
    DIRTY_SENS  = 1u << 1,
    DIRTY_ACT   = 1u << 2,
    DIRTY_SAFE  = 1u << 3,
    DIRTY_AUTO  = 1u << 4,
    DIRTY_SYS   = 1u << 5,
    DIRTY_RULES = 1u << 6,
    DIRTY_CROP  = 1u << 7,
    DIRTY_ALL   = 0xFFu,
};

/// Bir bölümü NVS'ten okur. Başarısızsa bölümü varsayılanda bırakır ve
/// **loglar** — sessiz düşüş yasak (ARCHITECTURE §16.4).
///
/// @return true = NVS'ten okundu · false = varsayılanda kaldı
bool loadSection(const char* key, void* dest, size_t expectedSize, const char* label)
{
    size_t        len = expectedSize;
    const ErrCode rc  = hal::nvsstore::getBlob(hal::NS_CONFIG, key, dest, len);

    if (rc == ErrCode::OK && len == expectedSize)
    {
        return true;
    }

    // Boyut uyuşmazlığı, kaydın eski bir şemadan kaldığını gösterir.
    core::diag::log(core::LogLevel::WARNING,
                    (rc == ErrCode::CFG_NOT_FOUND) ? ErrCode::CFG_NOT_FOUND : ErrCode::CFG_CORRUPT,
                    static_cast<int32_t>(len), label);
    ++g_result.sectionsDefaulted;
    return false;
}

ErrCode saveSection(const char* key, const void* src, size_t size)
{
    return hal::nvsstore::setBlob(hal::NS_CONFIG, key, src, size);
}

/// Eski şemadan göç.
///
/// **1 → 2 (ISSUE-021):** kural bölümü eklendi. Sürüm 1 kaydında kural
/// SAKLANMIYORDU, dolayısıyla taşınacak veri de yok: `load()` zaten
/// varsayılanlardan başladığı için kural kümesi boş kalır ve `DIRTY_ALL` ile
/// yeni şemaya yeniden yazılır. Dönüştürülecek bir alan olmadığı için burada
/// yalnızca kayıt düşülür.
///
/// **2 → 3 (TASK-067):** ürün profili bölümü eklendi. Aynı durum: sürüm 2'de
/// saklanabilecek bir ürün seçimi yoktu, dolayısıyla `crop` varsayılanda
/// (`CropId::NONE`) kalır ve cihaz TASK-067 öncesiyle birebir aynı davranır —
/// ürün seçilene kadar hiçbir kural üretilmez.
///
/// **AKTÜATÖR DİZİSİ BÜYÜDÜ (TASK-066).** `MAX_ACTUATORS` 4'ten 5'e çıktığı
/// için `cfg.act` bölümünün BAYT UZUNLUĞU değişti. Bu, ayrı bir göç adımı
/// gerektirmez: `loadSection()` uzunluk uyuşmazlığını zaten yakalar, bölümü
/// varsayılanda bırakır ve WARNING olarak loglar. Sonuç doğrudur — eski kayıt
/// yeni röleler için hiçbir değer taşımıyordu ve onların güvenli varsayılanı
/// `enabled = 0`'dır.
ErrCode migrate(uint16_t fromVersion, core::Config& cfg)
{
    (void)cfg;
    core::diag::log(core::LogLevel::INFO, ErrCode::OK, static_cast<int32_t>(fromVersion),
                    "config migration uygulandi");
    return ErrCode::OK;
}

} // namespace

ErrCode load()
{
    memset(&g_result, 0, sizeof(g_result));

    // Her zaman güvenli varsayılanlardan başla: bir bölüm okunamazsa
    // o bölüm zaten geçerli bir değerde kalır.
    core::loadDefaults(g_config);

    uint32_t storedVersion = 0;
    const ErrCode verRc    = hal::nvsstore::getU32(hal::NS_CONFIG, KEY_VERSION, storedVersion);

    if (verRc != ErrCode::OK)
    {
        // Hiç kayıt yok — ilk açılış. Varsayılanlarla devam.
        g_result.code      = ErrCode::CFG_NOT_FOUND;
        g_result.fullReset = 1;
        core::diag::log(core::LogLevel::WARNING, ErrCode::CFG_NOT_FOUND, 0,
                        "config kaydi yok — varsayilanlar kullaniliyor");
        g_dirtyMask = DIRTY_ALL;
        return ErrCode::CFG_NOT_FOUND;
    }

    g_result.storedVersion = static_cast<uint16_t>(storedVersion);

    if (storedVersion > core::CONFIG_SCHEMA_VERSION)
    {
        // Firmware geri alınmış: daha yeni bir şemayla yazılmış kaydı okuyamayız.
        g_result.code      = ErrCode::CFG_VERSION_NEWER;
        g_result.fullReset = 1;
        core::diag::log(core::LogLevel::WARNING, ErrCode::CFG_VERSION_NEWER,
                        static_cast<int32_t>(storedVersion),
                        "config semasi firmware'den yeni — varsayilana donuldu");
        g_dirtyMask = DIRTY_ALL;
        return ErrCode::CFG_VERSION_NEWER;
    }

    // --- Bölümleri tek tek yükle ---
    loadSection(KEY_NET,  &g_config.network,    sizeof(g_config.network),    "cfg.net");
    loadSection(KEY_SENS, &g_config.sensors,    sizeof(g_config.sensors),    "cfg.sens");
    loadSection(KEY_ACT,  &g_config.actuators,  sizeof(g_config.actuators),  "cfg.act");
    loadSection(KEY_SAFE, &g_config.safety,     sizeof(g_config.safety),     "cfg.safe");
    loadSection(KEY_AUTO, &g_config.automation, sizeof(g_config.automation), "cfg.auto");
    loadSection(KEY_SYS,  &g_config.system,     sizeof(g_config.system),     "cfg.sys");

    // Kural bölümü YALNIZCA sürüm 2 ve üstünde aranır. Sürüm 1 kayıtlarında
    // böyle bir anahtar hiç yazılmadı; aramak her yükseltmede yanıltıcı bir
    // "bölüm bozuk" uyarısı üretirdi. Kural kümesi boş varsayılanda kalır.
    if (storedVersion >= RULES_SINCE_VERSION)
    {
        loadSection(KEY_RULES, &g_config.rules, sizeof(g_config.rules), "cfg.rules");
    }

    // Ürün bölümü YALNIZCA sürüm 3 ve üstünde aranır (TASK-067).
    if (storedVersion >= CROP_SINCE_VERSION)
    {
        loadSection(KEY_CROP, &g_config.crop, sizeof(g_config.crop), "cfg.crop");
    }

    g_config.magic         = core::CONFIG_MAGIC;
    g_config.schemaVersion = core::CONFIG_SCHEMA_VERSION;

    if (storedVersion < core::CONFIG_SCHEMA_VERSION)
    {
        migrate(static_cast<uint16_t>(storedVersion), g_config);
        g_result.migrated = 1;
        g_dirtyMask       = DIRTY_ALL;  // yeni sürümle yeniden yazılmalı
    }

    // --- TAM DOĞRULAMA ---
    //
    // Bölümler ayrı kurtarıldığı için tek tek geçerli ama BİRLİKTE tutarsız
    // bir config oluşmuş olabilir. Yarı tutarlı config ile çalışmak, güvenlik
    // eşiklerinin beklenmedik kombinasyonlarda uygulanması demektir.
    const core::ConfigError ve = core::cfgvalid::validateAll(g_config);
    if (!ve.ok())
    {
        core::loadDefaults(g_config);
        g_result.code      = ve.code;
        g_result.fullReset = 1;
        g_dirtyMask        = DIRTY_ALL;

        core::diag::log(core::LogLevel::CRITICAL, ve.code, 0,
                        "config dogrulamasi basarisiz — TUMU varsayilana donduruldu");
        return ve.code;
    }

    g_result.code = (g_result.sectionsDefaulted > 0) ? ErrCode::CFG_CORRUPT : ErrCode::OK;
    return ErrCode::OK;
}

const core::Config& get()
{
    return g_config;
}

void copyRules(core::RuleSet& out)
{
    taskENTER_CRITICAL(&g_mux);
    out = g_config.rules;
    taskEXIT_CRITICAL(&g_mux);
}

void copyActuators(core::ActuatorConfig (&out)[core::MAX_ACTUATORS])
{
    taskENTER_CRITICAL(&g_mux);
    for (uint8_t i = 0; i < core::MAX_ACTUATORS; ++i)
    {
        out[i] = g_config.actuators[i];
    }
    taskEXIT_CRITICAL(&g_mux);
}

const ConfigLoadResult& lastLoadResult()
{
    return g_result;
}

// --- Güncelleme -------------------------------------------------------------
//
// Her güncelleme ÖNCE doğrular. Geçersiz değer ne RAM'e ne NVS'e yazılır.

core::ConfigError updateNetwork(const core::NetworkConfig& v)
{
    const core::ConfigError e = core::cfgvalid::validateNetwork(v);
    if (!e.ok())
    {
        return e;
    }
    g_config.network = v;
    g_dirtyMask |= DIRTY_NET;
    return core::configOk();
}

core::ConfigError updateSensor(uint8_t index, const core::SensorConfig& v)
{
    if (index >= core::MAX_SENSORS)
    {
        return core::ConfigError{ErrCode::CFG_VALIDATION_FAILED, "sensors.index"};
    }

    const core::ConfigError e = core::cfgvalid::validateSensor(v, index);
    if (!e.ok())
    {
        return e;
    }

    // ── ALANLAR ARASI: SEVİYE SENSÖRÜ KAPATILAMAZ ──────────────────────────
    // `requireLevelSensor` açıkken seviye sensörünü kapatmak, pompa güvenlik
    // kilidini SESSİZCE devre dışı bırakırdı. `validateAll` bunu zaten
    // yakalıyor ama orası ancak `persist()` sırasında çalışır; burada
    // reddetmek, geçersiz durumun RAM'e hiç girmemesini sağlar.
    if (index == static_cast<uint8_t>(core::SensorId::WATER_LEVEL) && v.enabled == 0u &&
        g_config.safety.requireLevelSensor != 0u)
    {
        return core::ConfigError{ErrCode::CFG_VALIDATION_FAILED, "sensors.waterLevel.enabled"};
    }

    // Yapı yazması kilit altında: `SensorService` `io_sense` task'ında
    // `validRange` ve `scale`/`offset` üçlüsünü birlikte okuyor.
    taskENTER_CRITICAL(&g_mux);
    g_config.sensors[index] = v;
    taskEXIT_CRITICAL(&g_mux);

    g_dirtyMask |= DIRTY_SENS;
    ++g_sensorsRevision;
    return core::configOk();
}

uint32_t sensorsRevision()
{
    return g_sensorsRevision;
}

core::ConfigError updateActuator(uint8_t index, const core::ActuatorConfig& v)
{
    const core::ConfigError e = core::cfgvalid::validateActuator(v, index);
    if (!e.ok())
    {
        return e;
    }
    // Yapı yazması KİLİT ALTINDA: `ActuatorManager` bu üçlüyü (`minRunMs`,
    // `maxRunMs`, `cooldownMs`) tek bir turda okuyor; karışık sürüm okumak,
    // hiç var olmamış bir koruma kombinasyonuyla çalışmak demektir.
    taskENTER_CRITICAL(&g_mux);
    g_config.actuators[index] = v;
    taskEXIT_CRITICAL(&g_mux);

    g_dirtyMask |= DIRTY_ACT;
    return core::configOk();
}

core::ConfigError updateSafety(const core::SafetyConfig& v)
{
    const core::ConfigError e = core::cfgvalid::validateSafety(v);
    if (!e.ok())
    {
        return e;
    }

    // Güvenlik eşiği değişiklikleri eski → yeni olarak loglanır:
    // sahada "pompa neden farklı davranıyor" sorusunun izi kalmalıdır.
    core::diag::log(core::LogLevel::WARNING, ErrCode::OK,
                    static_cast<int32_t>(v.flowVerifyDelayMs),
                    "guvenlik esikleri degistirildi");

    g_config.safety = v;
    g_dirtyMask |= DIRTY_SAFE;
    return core::configOk();
}

core::ConfigError updateAutomation(const core::AutomationConfig& v)
{
    const core::ConfigError e = core::cfgvalid::validateAutomation(v);
    if (!e.ok())
    {
        return e;
    }
    g_config.automation = v;
    g_dirtyMask |= DIRTY_AUTO;
    return core::configOk();
}

core::ConfigError updateRules(const core::RuleSet& v)
{
    // Kural kümesi BÜTÜN olarak doğrulanır: tek tek geçerli iki kural, aynı
    // aktüatörü aynı öncelikle hedeflediğinde birlikte GEÇERSİZDİR
    // (`validateRules` bunu yakalar). Kısmi güncelleme bu kontrolü atlardı.
    //
    // Eşikler sensörün geçerli aralığına göre denetlendiği için mevcut sensör
    // yapılandırması da verilir.
    const core::ConfigError e = core::cfgvalid::validateRules(v, g_config.sensors);
    if (!e.ok())
    {
        return e;
    }

    // 196 baytlık yazma KİLİT ALTINDA (TASK-072). `AutomationEngine` bu kümeyi
    // `app_core`'da alan alan okuyor; korumasız bırakılsaydı yarı eski yarı
    // yeni bir kural görebilirdi. Revizyon sayacı histerezis bayatlamasını
    // çözer ama yırtılmayı çözmez — ikisi farklı sorunlardır.
    taskENTER_CRITICAL(&g_mux);
    g_config.rules = v;
    taskEXIT_CRITICAL(&g_mux);

    g_dirtyMask |= DIRTY_RULES;
    ++g_rulesRevision;
    return core::configOk();
}

uint32_t rulesRevision()
{
    return g_rulesRevision;
}

core::ConfigError updateCrop(const core::CropConfig& v)
{
    const core::ConfigError e = core::cfgvalid::validateCrop(v);
    if (!e.ok())
    {
        return e;
    }

    // Ürün değişimi sahada "cihaz neden farklı sulama yapıyor" sorusunun
    // cevabıdır; güvenlik eşiği değişikliğiyle aynı gerekçeyle loglanır.
    core::diag::log(core::LogLevel::INFO, ErrCode::OK,
                    static_cast<int32_t>(static_cast<uint8_t>(v.crop)),
                    "urun profili secimi degisti");

    g_config.crop = v;
    g_dirtyMask |= DIRTY_CROP;
    return core::configOk();
}

core::ConfigError updateSystem(const core::SystemConfig& v)
{
    const core::ConfigError e = core::cfgvalid::validateSystem(v);
    if (!e.ok())
    {
        return e;
    }
    g_config.system = v;
    g_dirtyMask |= DIRTY_SYS;
    return core::configOk();
}

ErrCode persist()
{
    if (g_dirtyMask == 0u)
    {
        return ErrCode::OK;
    }

    // Yazmadan önce son bir doğrulama: geçersiz bir config asla kalıcılaşmaz.
    const core::ConfigError ve = core::cfgvalid::validateAll(g_config);
    if (!ve.ok())
    {
        core::diag::log(core::LogLevel::ERROR, ve.code, 0,
                        "gecersiz config kalicilastirilmadi");
        return ve.code;
    }

    ErrCode rc = ErrCode::OK;

    // Yalnızca DEĞİŞEN bölümler yazılır → gereksiz flash aşınması yok.
    if ((g_dirtyMask & DIRTY_NET) != 0)
    {
        rc = saveSection(KEY_NET, &g_config.network, sizeof(g_config.network));
    }
    if (rc == ErrCode::OK && (g_dirtyMask & DIRTY_SENS) != 0)
    {
        rc = saveSection(KEY_SENS, &g_config.sensors, sizeof(g_config.sensors));
    }
    if (rc == ErrCode::OK && (g_dirtyMask & DIRTY_ACT) != 0)
    {
        // KOPYA ÜZERİNDEN yazılır (TASK-072). `persist()` `store` task'ında
        // koşar; canlı yapıyı doğrudan NVS'e vermek, `net` task'ı tam o anda
        // yazarken yarı güncellenmiş bir bölümü FLASH'A KALICI hâle
        // getirebilirdi. Kilit yalnızca kopyalama süresince tutulur — NVS
        // yazması (milisaniyeler) kritik bölümün DIŞINDA kalır.
        core::ActuatorConfig acts[core::MAX_ACTUATORS];
        copyActuators(acts);
        rc = saveSection(KEY_ACT, acts, sizeof(acts));
    }
    if (rc == ErrCode::OK && (g_dirtyMask & DIRTY_SAFE) != 0)
    {
        rc = saveSection(KEY_SAFE, &g_config.safety, sizeof(g_config.safety));
    }
    if (rc == ErrCode::OK && (g_dirtyMask & DIRTY_AUTO) != 0)
    {
        rc = saveSection(KEY_AUTO, &g_config.automation, sizeof(g_config.automation));
    }
    if (rc == ErrCode::OK && (g_dirtyMask & DIRTY_RULES) != 0)
    {
        core::RuleSet rules;
        copyRules(rules);
        rc = saveSection(KEY_RULES, &rules, sizeof(rules));
    }
    if (rc == ErrCode::OK && (g_dirtyMask & DIRTY_CROP) != 0)
    {
        rc = saveSection(KEY_CROP, &g_config.crop, sizeof(g_config.crop));
    }
    if (rc == ErrCode::OK && (g_dirtyMask & DIRTY_SYS) != 0)
    {
        rc = saveSection(KEY_SYS, &g_config.system, sizeof(g_config.system));
    }

    if (rc == ErrCode::OK)
    {
        // Sürüm EN SON yazılır: bölümlerden biri yazılamazsa sürüm eski kalır
        // ve bir sonraki boot'ta tutarsız bir "yeni sürüm" iddiası olmaz.
        rc = hal::nvsstore::setU32(hal::NS_CONFIG, KEY_VERSION, core::CONFIG_SCHEMA_VERSION);
    }

    if (rc != ErrCode::OK)
    {
        core::diag::log(core::LogLevel::ERROR, rc, static_cast<int32_t>(g_dirtyMask),
                        "config yazilamadi");
        return rc;
    }

    g_dirtyMask = 0;
    return ErrCode::OK;
}

bool isDirty()
{
    return g_dirtyMask != 0u;
}

ErrCode factoryReset()
{
    const ErrCode cfgRc = hal::nvsstore::eraseNamespace(hal::NS_CONFIG);
    const ErrCode secRc = hal::secrets::clearAll();

    core::loadDefaults(g_config);
    g_dirtyMask = 0;

    // Kural kümesi de boşaldı: motorun eski kuralların çalışma durumuyla
    // devam etmemesi için revizyon ilerletilir.
    ++g_rulesRevision;

    core::diag::log(core::LogLevel::CRITICAL, ErrCode::OK, 0,
                    "FABRIKA AYARLARI — config ve sirlar silindi");

    return (cfgRc != ErrCode::OK) ? cfgRc : secRc;
}

} // namespace config
} // namespace services
