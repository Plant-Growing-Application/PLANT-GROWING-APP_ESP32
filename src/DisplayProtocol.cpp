#include "Define.h"
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
bool currentState = false;
bool previousState = false;
bool prevBackState = false;

bool bluetoothState = false;
bool wifiState = false;

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
    oled.print("IP:" + wifi.getLocalIPString());

    oled.setCursor(textX, ipY + 13);
    oled.print("MAC:AA:BB:CC:DD:EE:FF");

    // Saat ekle
    oled.setCursor(textX, ipY + 28);

    oled.print("TIME: 14:35");

    oled.display();
}
void DisplayProtocol::GoToPageWifi()
{
    CurrentPage = PAGE_WIFI;

    oled.clearDisplay();

    // Üst başlık
    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor((128 - 6 * 2 * 4) / 2, 0); // "WIFI" ortalama
    oled.print("WIFI");

    // Ortada bilgi: "WiFi: ON" veya "WiFi: OFF"
    oled.setTextSize(1);
    oled.setCursor(0, 28);
    oled.print("WiFi: ");
    StateWifi(wifiState);
    // Alt çizgi (metnin altına)
    int lineY = 38; // Yazı yüksekliğinin hemen altı
    oled.drawLine(0, lineY, 60, lineY, SSD1306_WHITE);
    oled.display();
}
void DisplayProtocol::GoToPageBluetooth()
{
    CurrentPage = PAGE_BLUETOOTH;

    oled.clearDisplay();

    // Üst başlık
    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor((128 - 6 * 2 * 9) / 2, 0); // "BLUETOOTH" ortalama
    oled.print("BLUETOOTH");

    // Ortada bilgi: "Bluetooth: ON" veya "Bluetooth: OFF"
    oled.setTextSize(1);
    oled.setCursor(0, 28); // Geniş metin için biraz sola alındı
    oled.print("Bluetooth: ");
    StateBluetooth(bluetoothState);
    // Alt çizgi (metnin altına)
    int lineY = 38; // Yazı yüksekliğinin hemen altı
    oled.drawLine(0, lineY, 80, lineY, SSD1306_WHITE);
    oled.display();
}
void DisplayProtocol::ChangePage(int encoderValue)
{
    if (encoderValue != previousEncoderState && !isInPage)
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
            GoToPageBluetooth();
            break;
        case PAGE_WIFI:
            GoToPageWifi(); // Varsayılan olarak WIFI kapalı
            break;
            // Diğer sayfalar için ekle
        }
    }
    else if (encoderValue != previousEncoderState && isInPage)
    {
        switch (CurrentPage)
        {
        case PAGE_BLUETOOTH:
            StateBluetooth(!bluetoothState);
            break;
        case PAGE_WIFI:
            StateWifi(!wifiState);
            break;
        default:
            break;
        }
    }
}

void DisplayProtocol::SelectedPage()
{
    // Buton durumu oku (LOW = basılı)
    bool currentState = !digitalRead(PIN_ENCODER_PUSH); // tersle, HIGH=basılmamış
    bool backState = !digitalRead(PIN_BACK_BUTTON);     // Exit tuşu

    // Sadece butona dokunduğun an tetikle
    if (currentState != previousState && CurrentPage != PAGE_INTRO)
    {
        isInPage = true;
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
    // EXIT (geri tuşu)
    if (backState == !prevBackState && isInPage)
    {
        isInPage = false;
        switch (CurrentPage)
        {
        case PAGE_INTRO:
            GoToPageIntro();
            break;
        case PAGE_BLUETOOTH:
            GoToPageBluetooth();
            break;
        case PAGE_WIFI:
            GoToPageWifi(); // Varsayılan olarak WIFI kapalı
            break;
        }

        // Durumları kaydet
        previousState = currentState;
        prevBackState = backState;
    }
}

void DisplayProtocol::StateBluetooth(bool btState)
{
    bluetoothState = btState;

    // Eğer şu an Bluetooth sayfasındaysak ekranı güncelle
    if (CurrentPage == PAGE_BLUETOOTH)
    {
        oled.fillRect(33, 28, 30, 10, SSD1306_BLACK); // Eski yazıyı sil (sadece o bölge)
        oled.setCursor(40, 28);                       // "ON/OFF" yazısının konumu
        oled.setTextColor(SSD1306_WHITE);
        oled.print(bluetoothState ? "ON" : "OFF");
        oled.display();
    }
}

void DisplayProtocol::StateWifi(bool wfState)
{
    wifiState = wfState; // ← küçük ama kritik düzeltme: "wifiState = wifiState" değil!

    // Eğer şu an WiFi sayfasındaysak ekranı güncelle
    if (CurrentPage == PAGE_WIFI)
    {
        oled.fillRect(33, 28, 30, 10, SSD1306_BLACK); // Bölgeyi temizle
        oled.setCursor(33, 28);                       // "ON/OFF" konumu
        oled.setTextColor(SSD1306_WHITE);
        oled.print(wifiState ? "ON" : "OFF");
        oled.display();
    }
}