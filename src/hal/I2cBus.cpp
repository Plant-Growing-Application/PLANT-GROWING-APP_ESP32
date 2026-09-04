#include "I2cBus.h"

#include <Wire.h>

#include "core/BoardPins.h"

namespace hal {
namespace i2cbus {
namespace {

bool g_ready = false;

} // namespace

core::ErrCode begin()
{
    if (g_ready)
    {
        return core::ErrCode::OK;
    }

    if (!Wire.begin(board::I2C_SDA, board::I2C_SCL))
    {
        return core::ErrCode::UI_DISPLAY_UNAVAILABLE;
    }

    // ── I2C HIZI: 400 kHz (FAST MODE) ──────────────────────────────────────
    //
    // `Wire.begin()` varsayilani 100 kHz'dir.
    //
    // SSD1306 tam karesi 1024 bayt. 100 kHz'de bir kare ~105 ms surer;
    // `ui` task periyodu 50 ms oldugu icin HER EKRAN DEGISIMI arayuzu iki
    // periyot boyunca bloklardi. Sahada "encoder gecisleri takiliyor" olarak
    // goruldu.
    //
    // 400 kHz'de ayni kare ~26 ms — periyodun yarisindan az. Ortam sensorleri
    // birkac bayt okur, onlar icin fark ihmal edilebilir; kritik olan OLED'dir.
    Wire.setClock(400000u);

    g_ready = true;
    return core::ErrCode::OK;
}

bool isReady()
{
    return g_ready;
}

} // namespace i2cbus
} // namespace hal
