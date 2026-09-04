#include "Bh1750.h"

#include <Wire.h>

#include "core/Diagnostics.h"
#include "hal/I2cBus.h"

namespace hal {
namespace bh1750 {
namespace {

using core::ErrCode;

// --- Komutlar (veri sayfası) ------------------------------------------------
constexpr uint8_t CMD_POWER_ON     = 0x01;
constexpr uint8_t CMD_RESET        = 0x07;
constexpr uint8_t CMD_CONT_HRES    = 0x10;  ///< sürekli, 1 lx çözünürlük, ~120 ms

/// Ham sayaç → lüks. Veri sayfasının ölçüm denklemi: lx = ham / 1.2
constexpr float COUNTS_PER_LUX = 1.2f;

bool     g_available = false;
uint16_t g_errors    = 0;

/// Sürekli mod başlatıldığı an (ms). İlk ölçüm hazır olana kadar okuma
/// reddedilir — hazır olmayan yazmaç 0 döndürür ve bu "gece" gibi görünür.
uint32_t g_startedAtMs = 0;

void noteError()
{
    if (g_errors < 0xFFFFu)
    {
        ++g_errors;
    }
    if (g_errors == ERROR_LIMIT && g_available)
    {
        g_available = false;
        core::diag::log(core::LogLevel::ERROR, ErrCode::SENSOR_OPEN_CIRCUIT,
                        static_cast<int32_t>(g_errors),
                        "BH1750 yanit vermiyor — isik sensoru devre disi");
    }
}

bool writeCommand(uint8_t cmd)
{
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(cmd);
    return Wire.endTransmission() == 0;
}

} // namespace

core::ErrCode begin()
{
    g_available = false;
    g_errors    = 0;

    const ErrCode busRc = i2cbus::begin();
    if (busRc != ErrCode::OK)
    {
        return busRc;
    }

    if (!writeCommand(CMD_POWER_ON))
    {
        return ErrCode::SENSOR_NOT_PRESENT;
    }

    // RESET yalnızca güç açıkken geçerlidir; sırası bu yüzden sabittir.
    // Yazmacı temizler — önceki oturumdan kalan bir değerin ilk okumada
    // "gerçek ölçüm" gibi görünmesini engeller.
    if (!writeCommand(CMD_RESET))
    {
        return ErrCode::SENSOR_NOT_PRESENT;
    }

    if (!writeCommand(CMD_CONT_HRES))
    {
        return ErrCode::SENSOR_NOT_PRESENT;
    }

    g_startedAtMs = millis();
    g_available   = true;
    return ErrCode::OK;
}

bool read(float& outLux)
{
    if (!g_available)
    {
        return false;
    }

    // İlk ölçüm henüz hazır değil: yazmaç 0 döner ve bu, gerçek bir "karanlık"
    // ölçümünden ayırt edilemez. Değer üretmek yerine "henüz yok" diyoruz;
    // çağıran bunu FAULT'a çevirir ve arayüz kısa süre "okunamıyor" gösterir.
    if ((millis() - g_startedAtMs) < FIRST_MEASUREMENT_MS)
    {
        return false;
    }

    if (Wire.requestFrom(static_cast<int>(I2C_ADDRESS), 2) != 2)
    {
        noteError();
        return false;
    }

    const uint16_t hi  = static_cast<uint16_t>(Wire.read());
    const uint16_t lo  = static_cast<uint16_t>(Wire.read());
    const uint16_t raw = static_cast<uint16_t>((hi << 8) | lo);

    g_errors = 0;
    outLux   = static_cast<float>(raw) / COUNTS_PER_LUX;
    return true;
}

bool isAvailable()
{
    return g_available;
}

uint16_t errorCount()
{
    return g_errors;
}

} // namespace bh1750
} // namespace hal
