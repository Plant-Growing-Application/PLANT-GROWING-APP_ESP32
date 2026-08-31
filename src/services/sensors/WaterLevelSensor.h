#pragma once

// Su seviyesi sensörü — TASK-026
//
// ██ GÜVENLİK ZİNCİRİNİN TEMELİ ██
//
// Pompanın çalışma izni doğrudan bu sensöre bağlıdır. Mevcut sistemde bu
// sensörün hiçbir izi yoktu (REQUIREMENTS §3.6) ve pompa hiçbir seviye
// koruması olmadan çalışabiliyordu.
//
// ── FAIL-SAFE KURALI (PAZARLIKSIZ) ─────────────────────────────────────────
// Seviye OKUNAMIYORSA "muhtemelen doludur" VARSAYILMAZ — YETERSİZ kabul edilir
// ve pompa kilitlenir (ARCHITECTURE §9.5, §12.2).
//
// Gerekçe: yanlış pozitif bir kilit ÜRETİM kaybıdır (pompa çalışmaz);
// yanlış negatif bir izin DONANIM kaybıdır (pompa kuru çalışır). İkisi
// eşdeğer değildir.
// ────────────────────────────────────────────────────────────────────────────
//
// TOPOLOJİ: iki bağımsız şamandıra (ISSUE-000). Tek sensör kullanılsaydı
// tek nokta hatası korumayı tamamen devre dışı bırakabilirdi.

#include <stdint.h>

#include "ISensor.h"
#include "core/Time.h"

namespace services {
namespace sensors {

/// Ayrık seviye durumu.
///
/// `SensorSample.value` sayısal kodu taşır (birim: `LEVEL_STATE`):
///   0.0 = CRITICAL · 1.0 = LOW_LEVEL · 2.0 = SUFFICIENT
///
/// Sayısal kod, `SensorSample`'ın tekdüze kalmasını sağlar ve
/// `SafetyMonitor`'ın eşik karşılaştırması yapabilmesine izin verir
/// (`value < 1.0` → kritik).
enum class WaterLevelState : uint8_t
{
    CRITICAL  = 0,  ///< pompa çalıştırılamaz
    LOW_LEVEL = 1,  ///< düşük ama kritik değil (isim `LOW` degil: Arduino `#define LOW`)
    SUFFICIENT = 2, ///< seviye yeterli
};

constexpr float levelToValue(WaterLevelState s)
{
    return static_cast<float>(static_cast<uint8_t>(s));
}

/// İki şamandıralı su seviyesi sensörü.
class WaterLevelSensor : public ISensor
{
public:
    core::ErrCode begin(const core::SensorConfig& cfg) override;
    RawSample     sample(const SampleContext& ctx) override;

    /// Son okunan ayrık durum — `SafetyMonitor` (TASK-030) için.
    WaterLevelState state() const { return _state; }

    /// Şamandıralar fiziksel olarak imkânsız bir kombinasyon gösterdi mi?
    ///
    /// Üst şamandıra suda yüzerken alttakinin kuru olması olamaz. Bu durumda
    /// EN AZ BİRİ arızalıdır; yazılım **her ikisini de** arızalı sayar.
    bool inconsistent() const { return _inconsistent; }

    /// Şamandıra debounce süresi.
    ///
    /// Mekanik kontaklar zıplar; debounce gerekir. Ancak her milisaniye
    /// güvenlik tepkisini geciktirir — 50 ms zıplamayı bastırmaya yeter,
    /// tepkiyi anlamlı ölçüde geciktirmez.
    static constexpr uint16_t DEBOUNCE_MS = 50;

private:
    /// Bir şamandıranın debounce'lu durumu.
    struct FloatSwitch
    {
        uint8_t  pin;
        bool     stable;        ///< true = su VAR
        bool     lastRaw;
        uint32_t lastChangeMs;
    };

    void updateSwitch(FloatSwitch& sw, uint32_t nowMs);

    FloatSwitch     _low{};
    FloatSwitch     _crit{};
    WaterLevelState _state        = WaterLevelState::CRITICAL;  // fail-safe başlangıç
    bool            _inconsistent = false;
    bool            _ready        = false;
};

} // namespace sensors
} // namespace services
