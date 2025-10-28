#include "Define.h"
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
bool currentState = false;
bool previousState = false;
int8_t previousEncoderState = 0;
void DisplayProtocol::GoToPageIntro()
{
    CurrentPage = PAGE_INTRO;

    oled.clearDisplay();

    oled.drawBitmap(0, 0, myLogo, 128, 64, SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);

    int textX = 0;
    int ipY = 28;

    oled.setCursor(textX, ipY);
    oled.print("IP:192.168.1.100");

    oled.setCursor(textX, ipY + 13);
    oled.print("MAC:AA:BB:CC:DD:EE:FF");

    // Saat ekle
    oled.setCursor(textX, ipY + 28);

    oled.print("TIME: 14:35");

    oled.display();
}
void DisplayProtocol::GoToPageWifi(bool wifiState)
{
    CurrentPage = PAGE_WIFI;

    oled.clearDisplay();

    // Üst başlık
    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor((128 - 6 * 2 * 4) / 2, 0); // "WIFI" ortalama
    oled.print("WIFI");

    // Ortada bilgi: "WiFi: ON" veya "WiFi: OFF"
    oled.setTextSize(2);
    oled.setCursor(20, 28);
    oled.print("WiFi: ");
    oled.print(wifiState ? "ON" : "OFF");

    oled.display();
}
void DisplayProtocol::GoToPageBluetooth(bool btState)
{
    CurrentPage = PAGE_BLUETOOTH;

    oled.clearDisplay();

    // Üst başlık
    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor((128 - 6 * 2 * 9) / 2, 0); // "BLUETOOTH" ortalama
    oled.print("BLUETOOTH");

    // Ortada bilgi: "Bluetooth: ON" veya "Bluetooth: OFF"
    oled.setTextSize(2);
    oled.setCursor(0, 28); // Geniş metin için biraz sola alındı
    oled.print("Bluetooth: ");
    oled.print(btState ? "ON" : "OFF");

    oled.display();
}
void DisplayProtocol::EncoderControl(int encoderValue)
{
    if (encoderValue != previousEncoderState)
    {
        previousEncoderState = encoderValue;

        if (encoderValue > 0)
        {
            CurrentPage = (CurrentPage + 1) % TOTAL_PAGES; // TOPLAM_SAYFA sabitini tanımla
        }
        else if (encoderValue < 0)
        {
            CurrentPage = (CurrentPage - 1 + TOTAL_PAGES) % TOTAL_PAGES;
        }

        // Yeni sayfaya git
        switch (CurrentPage)
        {
        case PAGE_INTRO:
            GoToPageIntro();
            break;
        case PAGE_BLUETOOTH:
            GoToPageBluetooth(true);
            break;
        case PAGE_WIFI:
            GoToPageWifi(false); // Varsayılan olarak WIFI kapalı
            break;
            // Diğer sayfalar için ekle
        }
    }
}

void DisplayProtocol::SelectedPage()
{
        // Buton durumu oku (LOW = basılı)
        bool currentState = !digitalRead(PIN_ENCODER_PUSH); // tersle, HIGH=basılmamış

        // Sadece butona dokunduğun an tetikle
        if (currentState && !previousState)
        {
            // Köşe çizgilerini çiz
            int lineLength = 8; // çizgi uzunluğu

            // Sol üst köşe
            oled.drawFastHLine(0, 0, lineLength, SSD1306_WHITE); // yatay
            oled.drawFastVLine(0, 0, lineLength, SSD1306_WHITE); // dikey

            // Sağ üst köşe
            oled.drawFastHLine(128 - lineLength, 0, lineLength, SSD1306_WHITE); // yatay
            oled.drawFastVLine(127, 0, lineLength, SSD1306_WHITE);              // dikey

            // Sol alt köşe
            oled.drawFastHLine(0, 63, lineLength, SSD1306_WHITE);              // yatay
            oled.drawFastVLine(0, 63 - lineLength, lineLength, SSD1306_WHITE); // dikey

            // Sağ alt köşe
            oled.drawFastHLine(128 - lineLength, 63, lineLength, SSD1306_WHITE); // yatay
            oled.drawFastVLine(127, 63 - lineLength, lineLength, SSD1306_WHITE); // dikey

            oled.display();
        }

        // Son durumu sakla
        previousState = currentState;
    }
