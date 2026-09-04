#include "RelayOutput.h"

#include <Arduino.h>

#include "core/BoardPins.h"

namespace hal {
namespace relay {
namespace {

using core::ActuatorId;

/// Mantıksal aktüatör → fiziksel pin eşlemesi.
///
/// `0xFF` = bu kimliğin fiziksel karşılığı yok. TASK-066 ile eşlenmemiş slot
/// KALMADI (ışık, ısıtıcı ve besin pompası gerçek pin aldı), ama nöbetçi değer
/// korunuyor: `pinOf()` aralık dışı bir kimlikle çağrılırsa sürülecek pin
/// olmadığını söylemenin tek yolu budur — alternatif, `kRelayPin` dizisinin
/// dışını okumaktı.
constexpr uint8_t PIN_UNMAPPED = 0xFFu;

/// Sıra `ActuatorId` ile AYNI olmak zorundadır; aşağıdaki `static_assert`'ler
/// bunu her satır için tek tek doğrular.
constexpr uint8_t kRelayPin[core::MAX_ACTUATORS] = {
    board::RELAY_WATER_PUMP,  // WATER_PUMP
    board::RELAY_AIR_PUMP,    // AIR_PUMP
    board::RELAY_GROW_LIGHT,  // GROW_LIGHT
    board::RELAY_HEATER,      // HEATER
    board::RELAY_NUTRIENT,    // NUTRIENT_PUMP
};

// Tablo ile enum'un kayması SESSİZ bir felakettir: "ışığı aç" komutu ısıtıcıyı
// çalıştırır. Her satır kimliğiyle eşleştirilerek derleme zamanında kilitlenir.
static_assert(kRelayPin[static_cast<uint8_t>(ActuatorId::WATER_PUMP)] ==
                  board::RELAY_WATER_PUMP,
              "kRelayPin[WATER_PUMP] yanlis pine bakiyor");
static_assert(kRelayPin[static_cast<uint8_t>(ActuatorId::AIR_PUMP)] ==
                  board::RELAY_AIR_PUMP,
              "kRelayPin[AIR_PUMP] yanlis pine bakiyor");
static_assert(kRelayPin[static_cast<uint8_t>(ActuatorId::GROW_LIGHT)] ==
                  board::RELAY_GROW_LIGHT,
              "kRelayPin[GROW_LIGHT] yanlis pine bakiyor");
static_assert(kRelayPin[static_cast<uint8_t>(ActuatorId::HEATER)] == board::RELAY_HEATER,
              "kRelayPin[HEATER] yanlis pine bakiyor");
static_assert(kRelayPin[static_cast<uint8_t>(ActuatorId::NUTRIENT_PUMP)] ==
                  board::RELAY_NUTRIENT,
              "kRelayPin[NUTRIENT_PUMP] yanlis pine bakiyor");

bool g_ready = false;

inline uint8_t pinOf(ActuatorId id)
{
    const uint8_t i = static_cast<uint8_t>(id);
    return (i < core::MAX_ACTUATORS) ? kRelayPin[i] : PIN_UNMAPPED;
}

/// Bir pini glitch'siz şekilde güvenli seviyede çıkışa alır.
///
/// SIRA KRİTİK: `pinMode(OUTPUT)` önce çağrılırsa, sürücü etkinleştiği anda
/// çıkış yazmacındaki ÖNCEKİ (tanımsız) değer sürülür — aktif-düşük bir
/// modülde bu, rölenin bir an çekilmesi demektir.
///
/// Önce yazmaca güvenli seviye yazılır, sonra sürücü açılır: sürücü açıldığı
/// anda doğru seviye zaten yerindedir.
void configureSafeOutput(uint8_t pin)
{
    digitalWrite(pin, board::RELAY_SAFE_LEVEL);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, board::RELAY_SAFE_LEVEL);
}

} // namespace

core::ErrCode begin()
{
    for (uint8_t i = 0; i < core::MAX_ACTUATORS; ++i)
    {
        const uint8_t pin = kRelayPin[i];
        if (pin != PIN_UNMAPPED)
        {
            configureSafeOutput(pin);
        }
    }
    g_ready = true;

    // NOT: burada log YOK. Bu fonksiyon boot Aşama 1'de, `Diagnostics`
    // hazır olmadan önce çalışabilir (ARCHITECTURE §7.1). Sonuç dönüş
    // değeriyle taşınır; boot yürütücüsü rapora yazar.
    return core::ErrCode::OK;
}

core::ErrCode set(ActuatorId id, bool on)
{
    const uint8_t pin = pinOf(id);
    if (pin == PIN_UNMAPPED)
    {
        return core::ErrCode::CFG_VALIDATION_FAILED;
    }
    if (!g_ready)
    {
        // Sürücü hazır değilken röle sürmek, güvenli seviyenin hiç
        // kurulmadığı anlamına gelir — reddedilir.
        return core::ErrCode::SYS_BOOT_STAGE_FAILED;
    }

    digitalWrite(pin, on ? board::RELAY_ACTIVE_LEVEL : board::RELAY_SAFE_LEVEL);
    return core::ErrCode::OK;
}

bool isOn(ActuatorId id)
{
    const uint8_t pin = pinOf(id);
    if (pin == PIN_UNMAPPED)
    {
        return false;
    }
    // GERÇEK pin seviyesi okunur — gölge değişkene güvenilmez.
    return digitalRead(pin) == board::RELAY_ACTIVE_LEVEL;
}

core::ErrCode allSafe()
{
    // Acil durum yolu: hızlı, bloklamayan, kısıt tanımayan.
    // `g_ready` kontrolü YAPILMAZ — güvenli seviyeye almak her koşulda
    // yapılabilmelidir.
    for (uint8_t i = 0; i < core::MAX_ACTUATORS; ++i)
    {
        const uint8_t pin = kRelayPin[i];
        if (pin != PIN_UNMAPPED)
        {
            digitalWrite(pin, board::RELAY_SAFE_LEVEL);
        }
    }
    return core::ErrCode::OK;
}

bool isMapped(ActuatorId id)
{
    return pinOf(id) != PIN_UNMAPPED;
}

} // namespace relay
} // namespace hal
