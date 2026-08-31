#include "OledPanel.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "core/BoardPins.h"
#include "core/Diagnostics.h"

namespace hal {
namespace oled {
namespace {

using core::ErrCode;

constexpr uint8_t  I2C_ADDRESS   = 0x3C;
constexpr int8_t   NO_RESET_PIN  = -1;
/// Bu kadar ardışık hatadan sonra panel kullanılamaz sayılır.
/// Sonsuz yeniden deneme YAPILMAZ — her döngüde başarısız bir I2C işlemi
/// `ui` task'ının periyodunu bozar.
constexpr uint16_t I2C_ERROR_LIMIT = 20;

Adafruit_SSD1306 g_panel(OLED_WIDTH, OLED_HEIGHT, &Wire, NO_RESET_PIN);

bool     g_available = false;
uint16_t g_i2cErrors = 0;

/// Aktarım sonrası hat sağlığını değerlendirir.
void noteTransfer(bool ok)
{
    if (ok)
    {
        g_i2cErrors = 0;
        return;
    }

    if (g_i2cErrors < 0xFFFFu)
    {
        ++g_i2cErrors;
    }

    if (g_i2cErrors == I2C_ERROR_LIMIT && g_available)
    {
        // Kablo kopmuş veya panel yanıt vermiyor. Sistemi kilitlemek yerine
        // paneli kullanılamaz işaretliyoruz — sistem çalışmaya devam eder.
        g_available = false;
        core::diag::log(core::LogLevel::ERROR, ErrCode::UI_DISPLAY_UNAVAILABLE,
                        static_cast<int32_t>(g_i2cErrors),
                        "OLED yanit vermiyor — ekran devre disi");
    }
}

} // namespace

core::ErrCode begin()
{
    Wire.begin(board::I2C_SDA, board::I2C_SCL);

    if (!g_panel.begin(SSD1306_SWITCHCAPVCC, I2C_ADDRESS))
    {
        // Mevcut sistem burada `while (true)` ile kilitleniyordu.
        // Ekransız bir sera cihazı hâlâ sulama yapabilir; kilitlenen yapamaz.
        core::diag::log(core::LogLevel::ERROR, ErrCode::UI_DISPLAY_UNAVAILABLE, 0,
                        "OLED baslatilamadi — sistem ekransiz devam ediyor");
        g_available = false;
        return ErrCode::UI_DISPLAY_UNAVAILABLE;
    }

    g_panel.clearDisplay();
    g_panel.setTextColor(SSD1306_WHITE);
    g_panel.display();

    g_available = true;
    g_i2cErrors = 0;
    return ErrCode::OK;
}

bool isAvailable()
{
    return g_available;
}

void clear()
{
    if (g_available)
    {
        g_panel.clearDisplay();
    }
}

void setTextSize(uint8_t size)
{
    if (g_available)
    {
        g_panel.setTextSize(size);
    }
}

void drawText(int16_t x, int16_t y, const char* text)
{
    if (!g_available || text == nullptr)
    {
        return;
    }
    g_panel.setCursor(x, y);
    g_panel.setTextColor(SSD1306_WHITE);
    g_panel.print(text);
}

void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (g_available)
    {
        g_panel.drawLine(x0, y0, x1, y1, SSD1306_WHITE);
    }
}

void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool filled)
{
    if (!g_available)
    {
        return;
    }
    if (filled)
    {
        g_panel.fillRect(x, y, w, h, SSD1306_BLACK);
    }
    else
    {
        g_panel.drawRect(x, y, w, h, SSD1306_WHITE);
    }
}

void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h)
{
    if (g_available && bitmap != nullptr)
    {
        g_panel.drawBitmap(x, y, bitmap, w, h, SSD1306_WHITE);
    }
}

uint16_t textWidth(const char* text, uint8_t size)
{
    if (text == nullptr)
    {
        return 0;
    }
    // Yerleşik 5x7 font + 1 piksel boşluk.
    uint16_t n = 0;
    while (text[n] != '\0')
    {
        ++n;
    }
    return static_cast<uint16_t>(n * 6u * (size == 0 ? 1u : size));
}

core::ErrCode display()
{
    if (!g_available)
    {
        return ErrCode::UI_DISPLAY_UNAVAILABLE;
    }
    g_panel.display();
    noteTransfer(true);
    return ErrCode::OK;
}

core::ErrCode displayRegion(int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (!g_available)
    {
        return ErrCode::UI_DISPLAY_UNAVAILABLE;
    }
    // Adafruit_SSD1306 kısmi aktarım sunmaz; bu sarmalayıcı arayüzü şimdiden
    // sabitler ki `ui` katmanı (TASK-052) kirli alan mantığını kurabilsin.
    // Ölçüm (TASK-062) tam aktarımın 50 ms bütçesini zorladığını gösterirse
    // burada gerçek kısmi aktarım uygulanacaktır.
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    g_panel.display();
    noteTransfer(true);
    return ErrCode::OK;
}

uint16_t i2cErrorCount()
{
    return g_i2cErrors;
}

} // namespace oled
} // namespace hal
