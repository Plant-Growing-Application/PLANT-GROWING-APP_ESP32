#include "Define.h"

int8_t previousEncoderState = 0;
GrowPlantClass GrowPlant;
extern String currentTime;
extern String currentIP;
extern String currentMAC;

extern WebServerManager webServer;
extern MyWiFi wifi;

// Sayfa değiştirme
void GrowPlantClass::ChangePage(int encoderValue)
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
        case PAGE_WPS:
            GoToPageWPS(); // Varsayılan olarak WIFI kapalı
            break;
            // Diğer sayfalar için ekle
        }
    }
    else if (encoderValue != previousEncoderState && isInPage)
    {
        switch (CurrentPage)
        {
        case PAGE_BLUETOOTH:
            StateBluetooth(!MyEeprom.Setting.IsBluetoothActive);
            break;
        case PAGE_WIFI:
            StateWifi(!MyEeprom.Setting.IsServerMode);
            break;
        case PAGE_WPS:
            StateWPS(!MyEeprom.Setting.IsWpsActive);
            break;
        default:
            break;
        }
    }
}

void GrowPlantClass::GoToPageIntro()
{
    CurrentPage = PAGE_INTRO;
    oled.clearDisplay();
    oled.drawBitmap(0, 0, myLogo, 128, 64, SSD1306_WHITE);

    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0, 28);
    oled.setCursor(0, 41);
    oled.display();
    SendWifiInfo();
}
void GrowPlantClass::GoToPageWifi()
{
    CurrentPage = PAGE_WIFI;

    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);

    // 🔸 Başlık
    oled.setTextSize(2);
    oled.setCursor((128 - 6 * 2 * 4) / 2, 0); // "WIFI" ortala
    oled.print("WIFI");

    // 🔸 Mod (SERVER / CLIENT)
    oled.setTextSize(1);
    oled.setCursor(0, 18);
    oled.print("MODE: ");
    oled.print(MyEeprom.Setting.IsServerMode ? "SERVER" : "CLIENT");

    // 🔸 Ayrım çizgisi
    oled.drawLine(0, 28, 127, 28, SSD1306_WHITE);

    // 🔸 SSID
    oled.setCursor(0, 34);
    if (MyEeprom.Setting.IsServerMode) // Server modu
    {
        oled.print("SSID: ESP32_SERVER"); // SoftAP SSID
        oled.setCursor(0, 44);
        oled.print("PASS: 12345678"); // SoftAP şifre
    }
    else if (!MyEeprom.Setting.IsServerMode) // Client modu
    {
        // 🔸 SSID ve Password
        oled.setCursor(0, 34);
        oled.print("SSID: ");
        oled.print(MyEeprom.Setting.SSID); // EEPROM’dan al

        oled.setCursor(0, 44);
        oled.print("PASS: ");
        oled.print(MyEeprom.Setting.Password); // EEPROM’dan al
    }

    ShowIP();
    oled.display();
}
void GrowPlantClass::GoToPageWPS()
{
    CurrentPage = PAGE_WPS;
    oled.clearDisplay();

    // Üst başlık: WPS
    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);
    String title = "WPS";

    int16_t x1, y1;
    uint16_t w, h;
    oled.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    int centerX = (oled.width() - w) / 2;
    int centerY = (oled.height() - h) / 4 - 10; // biraz yukarıdaf
    oled.setCursor(centerX, centerY);
    oled.print(title);
    oled.display();
    if (WiFi.status() == WL_CONNECTED)
    {
        oled.setTextSize(1);
        oled.fillRect(30, 35, 40, 10, SSD1306_BLACK); // Eski yazıyı sil (sadece o bölge)
        oled.setCursor(30, 35);
        oled.setTextColor(SSD1306_WHITE);
        oled.print("Internet Bagli");
        oled.display();
    }
    else
    {

        oled.setTextSize(1);
        oled.fillRect(30, 35, 40, 10, SSD1306_BLACK); // Eski yazıyı sil (sadece o bölge)
        oled.setCursor(30, 35);
        oled.setTextColor(SSD1306_WHITE);
        oled.print("Wps Baslat");
        oled.display();
    }
}

void GrowPlantClass::GoToPageBluetooth()
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
    StateBluetooth(MyEeprom.Setting.IsBluetoothActive);

    // Alt çizgi (metnin altına)
    int lineY = 38; // Yazı yüksekliğinin hemen altı
    oled.drawLine(0, lineY, 60, lineY, SSD1306_WHITE);

    oled.display();
}

void GrowPlantClass::ShowIP()
{
    // 🖥️ IP stringi güncelle
    String ipStr;
    if (MyEeprom.Setting.IsServerMode)
        ipStr = WiFi.softAPIP().toString(); // Server mod
    if (!MyEeprom.Setting.IsServerMode)
    {
        if (WiFi.status() == WL_CONNECTED)
            ipStr = WiFi.localIP().toString(); // Client mod
        else
            ipStr = "Connecting...";
    }

    // OLED güncelle
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    // Her sayfa dönüşünde sıfırdan çiz
    if (CurrentPage == PAGE_INTRO)
    {
        oled.fillRect(0, 28, 128, 10, SSD1306_BLACK);
        oled.setCursor(0, 28);
        oled.print("WiFi: " + ipStr);
    }
    else if (CurrentPage == PAGE_WIFI)
    {
        oled.fillRect(30, 58, 98, 8, SSD1306_BLACK); // IP alanını temizle
        oled.setCursor(0, 54);
        oled.print("IP: " + ipStr);
    }

    oled.display();
}

void GrowPlantClass::ShowMac(const String &macStr)
{
    if (macStr.isEmpty())
        return;
    oled.fillRect(0, 39, 128, 10, SSD1306_BLACK);
    oled.setCursor(0, 39);
    oled.print("MAC:" + macStr);
    oled.display();
}
void GrowPlantClass::ShowClock(const String &timeStr)
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

void GrowPlantClass::SelectedPage()
{
    // Buton durumu oku (LOW = basılı)
    IsEncoderPressed = digitalRead(PIN_ENCODER_PUSH);

    // Encoder basıldığında (HIGH->LOW geçişi yakala)
    if (IsEncoderPressed && !PreviousPressed && CurrentPage != PAGE_INTRO)
    {
        isInPage = !isInPage; // Sayfa içindeysek çık, dışındaysak gir

        if (isInPage)
        {
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
        else
        {
            // Sayfadan çıkınca, mevcut sayfayı yeniden yükle
            switch (CurrentPage)
            {
            case PAGE_INTRO:
                // GoToPageIntro();
                break;
            case PAGE_BLUETOOTH:
                GoToPageBluetooth();
                break;
            case PAGE_WIFI:
                GoToPageWifi();
                break;
            case PAGE_WPS:
                GoToPageWPS();
                break;
            }
        }
    }

    PreviousPressed = IsEncoderPressed;
}

void GrowPlantClass::StateBluetooth(bool btState)
{
    MyEeprom.Setting.IsBluetoothActive = btState;

    // Eğer şu an Bluetooth sayfasındaysak ekranı güncelle
    if (CurrentPage == PAGE_BLUETOOTH)
    {
        oled.fillRect(58, 28, 30, 10, SSD1306_BLACK); // Eski yazıyı sil (sadece o bölge)
        oled.setCursor(60, 28);                       // "ON/OFF" yazısının konumu
        oled.setTextColor(SSD1306_WHITE);
        oled.print(MyEeprom.Setting.IsBluetoothActive ? "ON" : "OFF");
        oled.display();
    }
}
void GrowPlantClass::StateWifi(bool wfState)
{
    MyEeprom.Setting.IsServerMode = !MyEeprom.Setting.IsServerMode;

    // Görsel kısım
    if (CurrentPage == PAGE_WIFI)
    {
        oled.fillRect(33, 18, 60, 10, SSD1306_BLACK);
        oled.setCursor(33, 18);
        oled.setTextColor(SSD1306_WHITE);
        oled.print(MyEeprom.Setting.IsServerMode ? "SERVER" : "CLIENT");

        // oled.print(MyEeprom.Setting.IsServerMode ? "CLIENT" : "SERVER");
        oled.display();
    }

    if (MyEeprom.Setting.IsServerMode)
    {
        // 🔹 SERVER + CLIENT aynı anda aktif olsun
        Serial.println("📡 SERVER MODE aktif (AP + STA)");
        WiFi.mode(WIFI_AP_STA); // 👈 kritik değişiklik

        // SoftAP başlat (ESP kendi ağı)
        WiFi.softAP("ESP32_SERVER", "12345678");

        IPAddress apIP = WiFi.softAPIP();
        Serial.print("🌐 AP IP: ");
        Serial.println(apIP);
        webServer.begin();
    }
    else
    {
        Serial.println("💻 CLIENT MODE aktif (STA)");
        WiFi.mode(WIFI_STA); // Sadece Station olarak ayarla
        if (WiFi.status() == WL_CONNECTED)
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
void GrowPlantClass::StateWPS(bool wpsState)
{
    MyEeprom.Setting.IsWpsActive = wpsState;
    if (CurrentPage == PAGE_WPS)
    {
        if (!WiFi.status() == WL_CONNECTED)
        {
            oled.setTextSize(1);
            oled.fillRect(30, 28, 40, 10, SSD1306_BLACK); // Eski yazıyı sil (sadece o bölge)
            oled.setCursor(30, 28);                       // "ON/OFF" yazısının konumu
            oled.setTextColor(SSD1306_WHITE);
            oled.print("Wps Baslat");
            oled.display();
        }
    }
}
void GrowPlantClass::SendWifiInfo()
{
    // 🔹 Wi-Fi modunu al (STA = client, AP = server)
    wifi_mode_t mode = WiFi.getMode();

    // ⏰ Saat her zaman güncellensin (bağlantı olmasa bile)
    currentTime = rtc.getFormattedTime();

    // 🌐 IP ve MAC sadece AP veya STA moddaysa alınsın
    if (mode == WIFI_AP || mode == WIFI_STA)
    {
        String ipNow = (mode == WIFI_AP) ? WiFi.softAPIP().toString()
                                         : WiFi.localIP().toString();

        if (currentIP != ipNow)
            currentIP = ipNow;

        if (currentMAC.isEmpty())
            currentMAC = WiFi.macAddress();
    }
    if (CurrentPage == PAGE_INTRO)
    {
        ShowClock(currentTime);
        ShowIP();
        ShowMac(currentMAC);
    }
}
