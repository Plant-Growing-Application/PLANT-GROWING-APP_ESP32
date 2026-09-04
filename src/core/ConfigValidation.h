#pragma once

// Konfigürasyon doğrulaması — TASK-014
//
// SAF FONKSİYONLAR: girdi config, çıktı hata. Yan etki, log, donanım yok.
// Bu sayede:
//   · host tarafında donanımsız test edilebilir (TASK-064)
//   · TASK-044 (API) **aynı** fonksiyonları çağırır — doğrulama mantığı
//     iki yerde yazılmaz, sınırlar API'den gevşetilemez
//
// HATA ALAN ADIYLA DÖNER: `ARCHITECTURE.md` §14.5 API hatalarının alan bazında
// gösterilmesini istiyor (`{error:{code, message, field}}`). Alan adı .rodata'da
// sabit metindir; ayırma yapılmaz.

#include "Config.h"
#include "ErrorCodes.h"

namespace core {

/// Doğrulama sonucu. `field` başarısızlıkta hangi alanın sorunlu olduğunu
/// gösterir; başarıda `nullptr`.
struct ConfigError
{
    ErrCode     code;
    const char* field;

    constexpr bool ok() const { return code == ErrCode::OK; }
};

constexpr ConfigError configOk()
{
    return ConfigError{ErrCode::OK, nullptr};
}

namespace cfgvalid {

/// Bölüm bazlı doğrulamalar — kısmi güncelleme (TASK-044 PATCH) için ayrı.
ConfigError validateNetwork(const NetworkConfig& c);
ConfigError validateSensor(const SensorConfig& c, uint8_t index);
ConfigError validateActuator(const ActuatorConfig& c, uint8_t index);
ConfigError validateSafety(const SafetyConfig& c);
ConfigError validateAutomation(const AutomationConfig& c);

/// Tek bir otomasyon kuralını doğrular (TASK-054).
///
/// `sensors` gerekli: eşiklerin ilgili sensörün geçerli aralığında olması
/// kontrol edilir — aralık dışı bir eşik ASLA tetiklenmez ve kullanıcı
/// kuralın neden çalışmadığını anlayamaz.
ConfigError validateRule(const Rule& r, uint8_t index, const SensorConfig* sensors);

/// Kural kümesini doğrular; aynı aktüatörü hedefleyen eşit öncelikli
/// kuralları da yakalar.
ConfigError validateRules(const RuleSet& rs, const SensorConfig* sensors);

/// Ürün seçimini doğrular (TASK-067).
///
/// Katalogda olmayan bir kimlik veya profilde bulunmayan bir dönem (yapraklı
/// üründe "meyve") reddedilir. Sessizce başka bir ürüne/döneme düşmek, ekranda
/// yazanla uygulanan parametrelerin ayrışması demektir.
ConfigError validateCrop(const CropConfig& c);

ConfigError validateSystem(const SystemConfig& c);

/// Tüm config + **alanlar arası tutarlılık**.
///
/// Tek tek geçerli ama birlikte anlamsız kombinasyonlar burada yakalanır:
///   · `minRunMs >= maxRunMs`
///   · `IpMode::STATIC` seçili ama gateway/subnet boş
///   · seviye sensörü zorunlu ama sensör devre dışı
ConfigError validateAll(const Config& c);

} // namespace cfgvalid
} // namespace core
