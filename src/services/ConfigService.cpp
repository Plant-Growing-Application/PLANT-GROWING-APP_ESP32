#include "ConfigService.h"

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
constexpr const char* KEY_SYS     = "sys";

core::Config     g_config{};
ConfigLoadResult g_result{};
uint8_t          g_dirtyMask = 0;

enum DirtyBit : uint8_t
{
    DIRTY_NET  = 1u << 0,
    DIRTY_SENS = 1u << 1,
    DIRTY_ACT  = 1u << 2,
    DIRTY_SAFE = 1u << 3,
    DIRTY_AUTO = 1u << 4,
    DIRTY_SYS  = 1u << 5,
    DIRTY_ALL  = 0x3Fu,
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

/// Eski şemadan göç. Şu an sürüm 1 tek sürüm olduğu için göç edilecek bir şey
/// yok; akış TASK-054 sürümü 2'ye çıkardığında devreye girecek.
///
/// Yol ŞİMDİ kuruluyor: sonradan eklemek, o noktada zaten kaydedilmiş kullanıcı
/// ayarlarını riske atardı.
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
    const core::ConfigError e = core::cfgvalid::validateSensor(v, index);
    if (!e.ok())
    {
        return e;
    }
    g_config.sensors[index] = v;
    g_dirtyMask |= DIRTY_SENS;
    return core::configOk();
}

core::ConfigError updateActuator(uint8_t index, const core::ActuatorConfig& v)
{
    const core::ConfigError e = core::cfgvalid::validateActuator(v, index);
    if (!e.ok())
    {
        return e;
    }
    g_config.actuators[index] = v;
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
        rc = saveSection(KEY_ACT, &g_config.actuators, sizeof(g_config.actuators));
    }
    if (rc == ErrCode::OK && (g_dirtyMask & DIRTY_SAFE) != 0)
    {
        rc = saveSection(KEY_SAFE, &g_config.safety, sizeof(g_config.safety));
    }
    if (rc == ErrCode::OK && (g_dirtyMask & DIRTY_AUTO) != 0)
    {
        rc = saveSection(KEY_AUTO, &g_config.automation, sizeof(g_config.automation));
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

    core::diag::log(core::LogLevel::CRITICAL, ErrCode::OK, 0,
                    "FABRIKA AYARLARI — config ve sirlar silindi");

    return (cfgRc != ErrCode::OK) ? cfgRc : secRc;
}

} // namespace config
} // namespace services
