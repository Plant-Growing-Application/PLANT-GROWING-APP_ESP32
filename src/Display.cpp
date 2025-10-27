#include "define.h"

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define OLED_RESET_PIN -1
#define OLED_I2C_ADDRESS 0x3C


extern int tempBrightness;
extern int brightness;

void initializeDisplay()
{
    Wire.begin(21, 22);
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS))
    {
        Serial.println(F("OLED bulunamadı!"));
        while (true)
            ; // sonsuz döngü
    }
}

void renderPage(int currentPage, bool isEditMode, int setpoint, int tempSetpoint,
                int volume, int tempVolume, int mode, int tempMode, const char *modeLabels[])
{
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0);

    switch (currentPage)
    {
    case 0:
        oled.print("Setpoint Ayari");
        oled.setCursor(0, 16);
        oled.printf("Deger: %d", isEditMode ? tempSetpoint : setpoint);
        break;
    case 1:
        oled.print("Volume Ayari");
        oled.setCursor(0, 16);
        oled.printf("Deger: %d", isEditMode ? tempVolume : volume);
        break;
    case 2:
        oled.print("Mod Secimi");
        oled.setCursor(0, 16);
        oled.printf("Mod: %s", modeLabels[isEditMode ? tempMode : mode]);
        break;
    case 3:
        oled.print("Parlaklik");
        oled.setCursor(0, 16);
        oled.printf("Deger: %d", isEditMode ? tempBrightness : brightness);
        break;
    }

    oled.setCursor(0, 45);
    oled.print(isEditMode ? "Enc:Deger  Push:Exit" : "Enc:Sayfa  Push:Edit");
    oled.display();
}

void showMessage(const char *message, uint16_t durationMs, int currentPage)
{
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);
    int16_t x1, y1;
    uint16_t w, h;
    oled.getTextBounds(message, 0, 0, &x1, &y1, &w, &h);
    int x = (DISPLAY_WIDTH - (int)w) / 2;
    oled.setCursor(x, 20);
    oled.print(message);
    oled.display();
    delay(durationMs);
}
