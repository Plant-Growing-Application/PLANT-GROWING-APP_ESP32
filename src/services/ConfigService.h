#pragma once

// Konfigürasyon yaşam döngüsü — TASK-015
//
// BÖLÜM BAŞINA AYRI NVS ANAHTARI: config tek blob olarak değil, altı ayrı
// anahtar olarak saklanır (`net`, `sens`, `act`, `safe`, `auto`, `sys`).
//
//   · Kısmi bozulmada YALNIZCA o bölüm varsayılana döner — kullanıcı Wi-Fi
//     ayarını kaybetmeden kalibrasyonunu geri alabilir
//   · Tek alan değişiminde yalnızca o bölüm yeniden yazılır → flash aşınması
//     azalır
//
// Mevcut sistemin `StoredData` tek blok deseni tam tersiydi: bir bayt bozulunca
// TÜM ayarlar gidiyordu (REQUIREMENTS §7.1).
//
// SESSİZ VARSAYILANA DÖNÜŞ YOK (ARCHITECTURE §16.4): her düşüş loglanır ve
// `lastLoadResult()` ile sorgulanabilir. Mevcut sistemdeki sessiz
// `memset(&Setting, 0, ...)` davranışının karşıtı.

#include <stdint.h>

#include "core/Config.h"
#include "core/ConfigValidation.h"
#include "core/ErrorCodes.h"

namespace services {

/// Yükleme sonucunun özeti — boot raporu ve arayüz için.
struct ConfigLoadResult
{
    core::ErrCode code;
    uint8_t       sectionsDefaulted;  ///< kaç bölüm varsayılana döndü
    uint8_t       migrated;           ///< 1 = eski sürümden göç edildi
    uint8_t       fullReset;          ///< 1 = tüm config varsayılana döndü
    uint16_t      storedVersion;      ///< NVS'te bulunan şema sürümü
};

namespace config {

/// NVS'ten yükler, doğrular, gerekiyorsa göç eder.
///
/// Akış (ARCHITECTURE §15.3):
/// ```
///   sürüm == mevcut  → doğrudan kullan
///   sürüm <  mevcut  → migration uygula, yeni sürümle yaz, INFO logla
///   sürüm >  mevcut  → varsayılana dön, WARNING logla (firmware geri alınmış)
///   kayıt yok/bozuk  → o BÖLÜM varsayılana döner, WARNING logla
/// ```
///
/// Bölümler ayrı kurtarıldığı için sonda **tam doğrulama** yapılır: tek tek
/// geçerli ama birlikte tutarsız bir config oluşmuşsa tümü varsayılana döner
/// ve CRITICAL loglanır. Yarı tutarlı config ile çalışmak, güvenlik eşiklerinin
/// beklenmedik kombinasyonlarda uygulanması demektir.
core::ErrCode load();

/// RAM'deki tek kopya. Okuyucular her döngüde bunu okur; ayrı bir değişiklik
/// bildirimi gerekmez (eşik değişikliği bir sonraki döngüde etkili olur).
const core::Config& get();

/// Son `load()` sonucunun özeti.
const ConfigLoadResult& lastLoadResult();

// --- Güncelleme -------------------------------------------------------------
//
// Her `update*` çağrısı ÖNCE doğrular. Geçersiz değer **asla kalıcılaşmaz**;
// RAM'deki kopya da değişmez.

core::ConfigError updateNetwork(const core::NetworkConfig& v);
core::ConfigError updateSensor(uint8_t index, const core::SensorConfig& v);
core::ConfigError updateActuator(uint8_t index, const core::ActuatorConfig& v);
core::ConfigError updateSafety(const core::SafetyConfig& v);
core::ConfigError updateAutomation(const core::AutomationConfig& v);
core::ConfigError updateSystem(const core::SystemConfig& v);

/// Değişmiş bölümleri NVS'e yazar.
///
/// **SENKRONDUR ve flash hızında çalışır.** AsyncTCP callback'inden veya
/// `app_core` döngüsünden ÇAĞIRMAYIN (ARCHITECTURE §14.6, P3).
/// Bu çağrı `store` task'ının bağlamına aittir (TASK-059).
core::ErrCode persist();

/// Yazılmayı bekleyen değişiklik var mı?
bool isDirty();

/// Config'i **ve sırları** siler, varsayılanlara döner.
///
/// Factory reset akışının parçasıdır; ardından kontrollü yeniden başlatma
/// yapılmalıdır (TASK-012).
core::ErrCode factoryReset();

} // namespace config
} // namespace services
