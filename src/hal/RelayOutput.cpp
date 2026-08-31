#include "RelayOutput.h"

#include <Arduino.h>

#include "core/BoardPins.h"

namespace hal {
namespace relay {
namespace {

using core::ActuatorId;

/// Mantıksal aktüatör → fiziksel pin eşlemesi.
///
/// `0xFF` = bu kimliğin fiziksel karşılığı yok. Yardımcı aktüatörler (AUX)
/// donanımda tanımlı değil; olmayan bir pini sürmeye çalışmak yerine
/// eşlenmemiş bırakılıyor (P7).
constexpr uint8_t PIN_UNMAPPED = 0xFFu;

constexpr uint8_t kRelayPin[static_cast<uint8_t>(ActuatorId::AUX_2) + 1] = {
    board::RELAY_WATER_PUMP,  // WATER_PUMP
    board::RELAY_AIR_PUMP,    // AIR_PUMP
    PIN_UNMAPPED,             // AUX_1
    PIN_UNMAPPED,             // AUX_2
};

bool g_ready = false;

inline uint8_t pinOf(ActuatorId id)
{
    const uint8_t i = static_cast<uint8_t>(id);
    return (i <= static_cast<uint8_t>(ActuatorId::AUX_2)) ? kRelayPin[i] : PIN_UNMAPPED;
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
    for (uint8_t i = 0; i <= static_cast<uint8_t>(ActuatorId::AUX_2); ++i)
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
    for (uint8_t i = 0; i <= static_cast<uint8_t>(ActuatorId::AUX_2); ++i)
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
