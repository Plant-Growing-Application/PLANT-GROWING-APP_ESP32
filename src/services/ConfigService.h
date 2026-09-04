#pragma once

// Konfigürasyon yaşam döngüsü — TASK-015
//
// BÖLÜM BAŞINA AYRI NVS ANAHTARI: config tek blob olarak değil, yedi ayrı
// anahtar olarak saklanır (`net`, `sens`, `act`, `safe`, `auto`, `rules`,
// `sys`). `rules` şema sürümü 2 ile eklendi (ISSUE-021).
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
///
/// ── YIRTIK OKUMA UYARISI (TASK-072) ────────────────────────────────────────
/// Bu referans CANLI yapıya işaret eder ve `net` task'ı ona yazarken
/// `app_core` okuyor olabilir. **Skaler alanlar güvenlidir**: 1/2/4 baytlık
/// hizalı okuma/yazma Xtensa'da atomiktir, dolayısıyla `safety.*`,
/// `automation.mode` ve benzerleri doğrudan okunabilir.
///
/// **Yapı okumaları güvenli DEĞİLDİR.** Bir `Rule` veya `ActuatorConfig`
/// alan alan okunurken karşı taraf tamamını değiştirebilir ve okuyucu iki
/// farklı sürümün alanlarını karıştırabilir. Bu iki yapı için aşağıdaki
/// kopyalama fonksiyonlarını kullanın.
const core::Config& get();

// --- Tutarlı kopyalar -------------------------------------------------------
//
// NEDEN KİLİT SERVİSTE: `core/` ve `domain/` FreeRTOS başlığı include edemez
// (D5). Kilit burada, L2'de yaşar; `domain/` yalnızca kopyalama fonksiyonunu
// çağırır — `rulesRevision()` için zaten kurulmuş olan aşağı yönlü bağımlılığın
// aynısı.
//
// Kilit bir SPINLOCK'tır ve yalnızca `memcpy` süresince tutulur (~0,5 µs).
// Bloklamaz, öncelik terslenmesi üretmez (ARCHITECTURE P3).

/// Kural kümesinin tutarlı bir kopyasını alır.
///
/// `AutomationEngine` her değerlendirme turunda bunu çağırır. Canlı yapıyı
/// doğrudan okusaydı, `PUT /api/config/rules` veya otomatik dönem ilerlemesi
/// tam o anda 196 baytlık kümeyi yeniden yazarken yarı eski yarı yeni bir
/// kural görebilirdi.
void copyRules(core::RuleSet& out);

/// Aktüatör kısıtlarının tutarlı bir kopyasını alır.
///
/// `ActuatorManager` her `apply()` turunda çağırır. `minRunMs`/`maxRunMs`/
/// `cooldownMs` üçlüsünün karışık sürümlerini okumak, koruma sürelerinin
/// hiç var olmamış bir kombinasyonuyla çalışmak demektir.
void copyActuators(core::ActuatorConfig (&out)[core::MAX_ACTUATORS]);

/// Son `load()` sonucunun özeti.
const ConfigLoadResult& lastLoadResult();

// --- Güncelleme -------------------------------------------------------------
//
// Her `update*` çağrısı ÖNCE doğrular. Geçersiz değer **asla kalıcılaşmaz**;
// RAM'deki kopya da değişmez.

core::ConfigError updateNetwork(const core::NetworkConfig& v);

/// Bir sensörün yapılandırmasını değiştirir (etkinlik + kalibrasyon).
///
/// `sensorsRevision()` artırılır; `SensorService` bunu izleyerek YENİ
/// ETKİNLEŞTİRİLEN sensörün sürücüsünü çalışma anında başlatır. Bu bağ
/// olmadan bir sensörü açmak yeniden başlatmayı gerektirirdi ve arayüz bunu
/// kullanıcıya söylemiyordu (ISSUE-035).
core::ConfigError updateSensor(uint8_t index, const core::SensorConfig& v);

/// Sensör yapılandırması her değiştiğinde artar. `SensorService` okur.
///
/// Kural kümesindeki `rulesRevision()` ile aynı desen: tek yazarlı sayaç,
/// okuyucu tarafta kilit gerekmez.
uint32_t sensorsRevision();
core::ConfigError updateActuator(uint8_t index, const core::ActuatorConfig& v);
core::ConfigError updateSafety(const core::SafetyConfig& v);
core::ConfigError updateAutomation(const core::AutomationConfig& v);

/// Ürün profili seçimini değiştirir (TASK-067).
///
/// **Kural kümesine DOKUNMAZ.** Seçim ile kuralların üretilmesi ayrı iki
/// adımdır: `services::crop::apply()` önce kuralları üretip `updateRules()`
/// ile doğrulatır, ancak ikisi de geçerse seçimi buraya yazar. Tek adımda
/// yapılsaydı, geçersiz bir kural kümesi üretildiğinde config'te "çilek
/// seçili ama çilek kuralları yok" gibi tutarsız bir durum kalırdı.
core::ConfigError updateCrop(const core::CropConfig& v);

core::ConfigError updateSystem(const core::SystemConfig& v);

/// Kural kümesini **bütün olarak** değiştirir (ISSUE-021).
///
/// Kısmi güncelleme (tek kural yazma) bilinçli olarak YOK: çakışma kontrolü
/// (aynı aktüatör + aynı öncelik) yalnızca küme bütününde anlamlıdır ve
/// slot slot yazmak, aradaki geçici durumda geçersiz bir küme bırakırdı.
core::ConfigError updateRules(const core::RuleSet& v);

/// Kural kümesinin sürüm sayacı — her `updateRules()` ve fabrika sıfırlaması
/// sonrası artar. Otomasyon motoru bunu izleyerek kural çalışma durumlarını
/// sıfırlar (bkz. `domain/AutomationEngine.h`).
uint32_t rulesRevision();

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
