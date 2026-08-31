#include "Define.h"
#include <LittleFS.h>

int8_t previousEncoderState = 0;
GrowPlantClass GrowPlant;
extern String currentTime;
extern String currentIP;
extern String currentMAC;

unsigned long CurrenMillis = 0;
unsigned long PreviousMillis = 0;

extern MyWiFi wifi;

// Extern functions for WiFiMonitor task control
extern void pauseWiFiMonitor();
extern void resumeWiFiMonitor();
// Extern function for setting WiFi mode
extern void setWiFiMode(wifi_mode_t mode, bool isServerMode);
/////////////////////////////////////////////TEST/////////////////////////////////////////////////

unsigned long previousMillis = 0;
const unsigned long interval = 1000; // 200 ms

/////////////////////////////////////////////TEST/////////////////////////////////////////////////

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
        case PAGE_WIFI:
            GoToPageWifi(); // Varsayılan olarak WIFI kapalı
            break;
        case PAGE_WPS:
            GoToPageWPS(); // Varsayılan olarak WIFI kapalı
            break;
        case PAGE_SENSORS:
            GoToPageSensors();
            break;
            // Diğer sayfalar için ekle
        }
    }
    else if (encoderValue != previousEncoderState && isInPage)
    {
        switch (CurrentPage)
        {
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
    oled.display();
    SendWifiInfo();
}
void GrowPlantClass::GoToPageWifi()
{
    CurrentPage = PAGE_WIFI;

    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);

    oled.setTextSize(2);
    oled.setCursor((128 - 6 * 2 * 4) / 2, 0);
    oled.print("WIFI");

    oled.setTextSize(1);
    oled.setCursor(0, 18);
    oled.print("MODE: ");
    oled.print(MyEeprom.Setting.IsServerMode ? "SERVER" : "CLIENT");

    oled.drawLine(0, 28, 127, 28, SSD1306_WHITE);

    oled.setCursor(0, 34);
    if (MyEeprom.Setting.IsServerMode)
    {
        oled.print("SSID: ESP32_SERVER");
        oled.setCursor(0, 44);
        oled.print("PASS: 12345678");
    }
    else if (!MyEeprom.Setting.IsServerMode)
    {
        oled.setCursor(0, 34);
        oled.print("SSID: ");
        oled.print(MyEeprom.Setting.SSID);

        oled.setCursor(0, 44);
        oled.print("PASS: ");
        oled.print(MyEeprom.Setting.Password);
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

void GrowPlantClass::GoToPageSensors()
{
    CurrentPage = PAGE_SENSORS;
    oled.clearDisplay();

    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);

    int16_t x1, y1;
    uint16_t w, h;
    String title = "SENSORS";
    oled.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);

    int centerX = (oled.width() - w) / 2;
    oled.setCursor(centerX, 0);
    oled.print(title);

    oled.setTextSize(1);

    oled.fillRect(0, 28, 128, 10, SSD1306_BLACK);
    oled.setCursor(0, 28);
    oled.print("WaterFlow: ");
    oled.print(Sensor.WaterFlow);

    int lineY = 38;
    oled.drawLine(0, lineY, 60, lineY, SSD1306_WHITE);

    oled.fillRect(0, 48, 128, 10, SSD1306_BLACK);
    oled.setCursor(0, 48);
    oled.print("WaterTemp: ");
    oled.print(Sensor.WaterTemprature);

    int lineY1 = 58;
    oled.drawLine(0, lineY1, 60, lineY1, SSD1306_WHITE);

    oled.display();
}
void GrowPlantClass::ShowIP()
{
    String ipStr;

    if (WiFi.status() == WL_CONNECTED)
        ipStr = currentIP;
    else
        ipStr = "Connecting...";

    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    if (CurrentPage == PAGE_INTRO)
    {
        oled.fillRect(0, 28, 128, 10, SSD1306_BLACK);
        oled.setCursor(0, 28);
        oled.print("WiFi: " + ipStr);
    }
    else if (CurrentPage == PAGE_WIFI)
    {
        oled.fillRect(30, 54, 98, 8, SSD1306_BLACK);
        oled.setCursor(0, 54);
        oled.print("IP: " + ipStr);
    }
}

void GrowPlantClass::ShowMac(const String &macStr)
{
    if (macStr.isEmpty())
        return;
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.fillRect(0, 39, 128, 10, SSD1306_BLACK);
    oled.setCursor(0, 39);
    oled.print("MAC:" + macStr);
}
void GrowPlantClass::ShowClock(const String &timeStr)
{
    if (timeStr.isEmpty())
        return;
    int16_t x1, y1;
    uint16_t w, h;
    oled.setTextSize(1);
    oled.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
    int rightX = oled.width() - w - 2;
    oled.setTextColor(SSD1306_WHITE);
    oled.fillRect(rightX, 0, w + 2, h + 2, SSD1306_BLACK);
    oled.setCursor(rightX, 0);
    oled.print(timeStr);
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
            case PAGE_WIFI:
                GoToPageWifi();
                break;
            case PAGE_WPS:
                GoToPageWPS();
                break;
            case PAGE_SENSORS:
                GoToPageSensors();
                break;
            }
        }
    }

    PreviousPressed = IsEncoderPressed;
}

void GrowPlantClass::StateWifi(bool wfState)
{
    // Pause the WiFiMonitor task to prevent interference while changing mode
    pauseWiFiMonitor();

    // Toggle the server mode and set the WiFi mode accordingly
    bool newServerMode = !MyEeprom.Setting.IsServerMode;
    wifi_mode_t newMode = newServerMode ? WIFI_AP_STA : WIFI_STA;
    setWiFiMode(newMode, newServerMode);

    // Update the OLED display if we are on the WIFI page
    if (CurrentPage == PAGE_WIFI)
    {
        oled.fillRect(33, 18, 60, 10, SSD1306_BLACK);
        oled.setCursor(33, 18);
        oled.setTextColor(SSD1306_WHITE);
        oled.print(MyEeprom.Setting.IsServerMode ? "SERVER" : "CLIENT");
        oled.display();
        // Settings are already saved by setWiFiMode, so no need to save again
    }

    // Resume the WiFiMonitor task
    resumeWiFiMonitor();
}
void GrowPlantClass::StateWPS(bool wpsState)
{
    MyEeprom.Setting.IsWpsActive = wpsState;
    if (CurrentPage == PAGE_WPS)
    {
        if (WiFi.status() == WL_CONNECTED)
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
    currentTime = rtc.getFormattedTime();

    if (MyEeprom.Setting.IsServerMode)
    {
        String ipSta = WiFi.softAPIP().toString();
        if (currentIP != ipSta)
            currentIP = ipSta;

        if (currentMAC.isEmpty())
            currentMAC = WiFi.macAddress();
    }
    else
    {
        String ipLocal = WiFi.localIP().toString();
        if (currentIP != ipLocal)
            currentIP = ipLocal;

        if (currentMAC.isEmpty())
            currentMAC = WiFi.macAddress();
    }
    if (CurrentPage == PAGE_INTRO)
    {
        ShowClock(currentTime);
        ShowIP();
        ShowMac(currentMAC);
        oled.display();
    }
}
