#include "Define.h"
int8_t previousEncoderState = 0;

bool bluetoothState = false;
bool wifiState = false;

// Sayfa değiştirme
void GrowPlant::ChangePage(int encoderValue)
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
        } // Yeni sayfaya git
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

void GrowPlant::GoToPageIntro()
{
    CurrentPage = PAGE_INTRO;
    oled.clearDisplay();
    oled.drawBitmap(0, 0, myLogo, 128, 64, SSD1306_WHITE);

    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0, 28);
    oled.setCursor(0, 41);
    oled.display();
}
void GrowPlant::GoToPageWifi()
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
void GrowPlant::GoToPageBluetooth()
{
    CurrentPage = PAGE_BLUETOOTH;
    oled.clearDisplay();

    // Üst başlık
    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);

    // Yazının boyutlarını al
    int16_t x1, y1;
    uint16_t w, h;
    String title = "BLUETOOTH";
    oled.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);

    // Ortalamayı hesapla
    int centerX = (oled.width() - w) / 2;
    oled.setCursor(centerX, 0);
    oled.print(title);

    // Ortada bilgi
    oled.setTextSize(1);
    oled.setCursor(0, 28);
    oled.print("Bluetooth: ");
    StateBluetooth(bluetoothState);

    // Alt çizgi (metnin altına)
    int lineY = 38; // Yazı yüksekliğinin hemen altı
    oled.drawLine(0, lineY, 60, lineY, SSD1306_WHITE);

    oled.display();
}

void GrowPlant::ShowIP()
{
    if (CurrentPage != PAGE_INTRO)
        return; // sadece intro sayfasında çalışsın

    String ipStr = wifi.getLocalIPString();
    if (ipStr.length() == 0 || ipStr == "0.0.0.0")
        return; // IP daha alınmamışsa boş döner, ekrana yazma

    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);

    // IP yazısının pozisyonu (intro sayfanla uyumlu olacak şekilde)
    int textX = 0;
    int ipY = 28;

    // Eski IP alanını temizle (çakışma olmasın diye)
    oled.fillRect(textX, ipY, 128, 10, SSD1306_BLACK);

    oled.setCursor(textX, ipY);
    oled.print("IP: " + ipStr);
    oled.display();
}

void GrowPlant::ShowMac()
{
    if (CurrentPage != PAGE_INTRO)
        return; // sadece intro sayfasında çalışsın

    String macStr = WiFi.macAddress();

    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);

    int textX = 0;
    int macY = 39; // IP'nin altına yerleştir

    // Eski MAC alanını temizle
    oled.fillRect(textX, macY, 128, 10, SSD1306_BLACK);

    oled.setCursor(textX, macY);
    oled.print("MAC:" + macStr);
    oled.display();
}
void GrowPlant::ShowClock()
{
    if (CurrentPage != PAGE_INTRO)
        return; // Sadece intro sayfasında çalışsın
    String timeStr = rtc.getFormattedTime();
    // Zaman yazı ayarları
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    // Yazının genişliğini hesapla
    int16_t x1, y1;
    uint16_t w, h;
    oled.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
    int rightX = oled.width() - w - 2; // Sağdan 2 piksel boşluk
    int topY = 0;
    // Eski zamanı silmek için o bölgeyi temizle
    oled.fillRect(rightX, topY, w + 2, h + 2, SSD1306_BLACK);
    // Yeni zamanı yaz
    oled.setCursor(rightX, topY);
    oled.print(timeStr);
    oled.display();
}

void GrowPlant::SelectedPage()
{
    // Buton durumu oku (LOW = basılı)
    IsEncoderPressed = digitalRead(PIN_ENCODER_PUSH); // tersle, HIGH=basılmamış
    IsBackPressed = !digitalRead(PIN_BACK_BUTTON);    // Exit tuşu     // Sadece butona dokunduğun an tetikle
    if (IsEncoderPressed && !PreviousPressed && CurrentPage != PAGE_INTRO)
    {
        isInPage = true;
        int lineLength = 7;
        // Köşe çizgilerini çiz
        // Sol üst
        oled.drawFastHLine(0, 0, lineLength, SSD1306_WHITE);
        oled.drawFastVLine(0, 0, lineLength, SSD1306_WHITE);

        // Sağ üst
        oled.drawFastHLine(128 - lineLength, 0, lineLength, SSD1306_WHITE);
        oled.drawFastVLine(127 - 1, 0, lineLength, SSD1306_WHITE);

        // Sol alt
        oled.drawFastHLine(0, 63 - 1, lineLength, SSD1306_WHITE);
        oled.drawFastVLine(0, 63 - lineLength, lineLength, SSD1306_WHITE);

        // Sağ alt
        oled.drawFastHLine(128 - lineLength, 63 - 1, lineLength, SSD1306_WHITE);
        oled.drawFastVLine(127 - 1, 63 - lineLength, lineLength, SSD1306_WHITE);
        oled.display();
    }
    // EXIT (geri tuşu)
    if (IsBackPressed && !PrevBackPressed && isInPage)
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
        } // Durumları kaydet
    }
    PreviousPressed = IsEncoderPressed;
    PrevBackPressed = IsBackPressed;
}

void GrowPlant::StateBluetooth(bool btState)
{
    bluetoothState = btState;

    // Eğer şu an Bluetooth sayfasındaysak ekranı güncelle
    if (CurrentPage == PAGE_BLUETOOTH)
    {
        oled.fillRect(58, 28, 30, 10, SSD1306_BLACK); // Eski yazıyı sil (sadece o bölge)
        oled.setCursor(60, 28);                       // "ON/OFF" yazısının konumu
        oled.setTextColor(SSD1306_WHITE);
        oled.print(bluetoothState ? "ON" : "OFF");
        oled.display();
    }
}
void GrowPlant::StateWifi(bool wfState)
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
