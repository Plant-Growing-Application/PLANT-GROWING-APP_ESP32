#include "Define.h"
#include "WebServer.h"
int8_t previousEncoderState = 0;

bool bluetoothState = false;
bool wifiState = false;
extern bool isServerMode;
extern String currentTime;
extern String currentIP;
extern String currentMAC;

extern WebServerManager webServer;
extern MyWiFi wifi;

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
    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor((128 - 6 * 2 * 4) / 2, 0);
    oled.print("WIFI");

    oled.setTextSize(1);
    oled.setCursor(0, 28);
    oled.print("WiFi: ");
    oled.print(wifiState ? "SERVER" : "CLIENT");

    oled.drawLine(0, 38, 60, 38, SSD1306_WHITE);
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

void GrowPlant::ShowIP(const String &ipStr)
{
    if (CurrentPage != PAGE_INTRO)
        return;

    if (ipStr == "0.0.0.0" || ipStr.isEmpty())
        return;

    // 📡 Mod türüne göre IP belirle
    String displayIP;
    if (WiFi.getMode() == WIFI_AP)
        displayIP = WiFi.softAPIP().toString(); // Server mod (ESP32 Access Point)
    else if (WiFi.getMode() == WIFI_STA)
        displayIP = WiFi.localIP().toString(); // Client mod (modeme bağlı)
    else
        displayIP = "0.0.0.0";

    // 🖥️ OLED güncelleme
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);

    // Her sayfa dönüşünde sıfırdan çiz
    oled.fillRect(0, 28, 128, 10, SSD1306_BLACK);
    oled.setCursor(0, 28);
    oled.print("WiFi: " + displayIP);

    oled.display();
}

void GrowPlant::ShowMac(const String &macStr)
{
    if (macStr.isEmpty())
        return;
    oled.fillRect(0, 39, 128, 10, SSD1306_BLACK);
    oled.setCursor(0, 39);
    oled.print("MAC:" + macStr);
    oled.display();
}
void GrowPlant::ShowClock(const String &timeStr)
{
    if (timeStr.isEmpty())
        return;
    int16_t x1, y1;
    uint16_t w, h;
    oled.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
    int rightX = oled.width() - w - 2;
    oled.fillRect(rightX, 0, w + 2, h + 2, SSD1306_BLACK);
    oled.setCursor(rightX, 0);
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
    wifiState = wfState;
    isServerMode = wifiState;

    // Görsel kısım
    if (CurrentPage == PAGE_WIFI)
    {
        oled.fillRect(33, 28, 60, 10, SSD1306_BLACK);
        oled.setCursor(33, 28);
        oled.setTextColor(SSD1306_WHITE);
        oled.print(wifiState ? "SERVER" : "CLIENT");
        oled.display();
    }

    // 🌐 Gerçek işlevsel kısım
    if (wifiState)
    {
        // Server Mode (ESP kendi WiFi’sini kuracak)
        Serial.println("📡 SERVER MODE aktif");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32_SERVER", "12345678");
        webServer.begin();
    }
    else
    {
        // Client Mode (mevcut modem)
        Serial.println("🌐 CLIENT MODE aktif");
        WiFi.mode(WIFI_STA);
        WiFi.begin("TP-Link_CDE6", "79222006");
        if (wifi.connect(5000))
        {
            Serial.println("✅ WiFi bağlandı");
            webServer.begin();
        }
        else
        {
            Serial.println("❌ WiFi bağlanamadı");
        }
    }
}
