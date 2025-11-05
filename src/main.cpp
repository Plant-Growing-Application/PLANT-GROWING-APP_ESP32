#include "define.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "WebServer.h"

wifi_mode_t mode = WiFi.getMode();
GrowPlant growPlant;
DisplayProtocol DpProtocol;
MyWiFi wifi("wifi");
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WebServerManager webServer(server, ws);
bool isServerMode = true;
RealTimeClock rtc("pool.ntp.org", 10800, 0); // GMT+3

// 💡 Global cache (ekrana hızlı yazmak için)
String currentIP = "";
String currentMAC = "";
String currentTime = "";
void Task_Display(void *pvParameters)
{
    for (;;)
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

            rtc.begin();
        }

        // 🖥️ Sadece intro sayfasında ekrana yaz
        if (growPlant.CurrentPage == PAGE_INTRO)
        {
            growPlant.ShowClock(currentTime);
            growPlant.ShowIP(currentIP);
            growPlant.ShowMac(currentMAC);
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 saniyede bir güncelle
    }
}

void Task_WiFiMonitor(void *pvParameters)
{
    for (;;)
    {
        if (!isServerMode) // sadece client modda kontrol et
        {
            if (WiFi.status() != WL_CONNECTED)
            {
                WiFi.reconnect();
                Serial.println("🔁 WiFi yeniden bağlanıyor...");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
void Task_WifiLed(void *pvParameters)
{
    for (;;)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            digitalWrite(WIFI_LED, HIGH);
            vTaskDelay(pdMS_TO_TICKS(200));
            digitalWrite(WIFI_LED, LOW);
            vTaskDelay(pdMS_TO_TICKS(200)); // bağlıysa hızlı yanıp sönme
        }
        else
        {
            digitalWrite(WIFI_LED, LOW); // bağlantı yoksa kapalı
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void setup()
{
    Serial.begin(115200);
    Wire.begin(21, 22);

    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS))
        while (true)
            ;

    DpProtocol.SetupEncoder(PIN_ENCODER_A, PIN_ENCODER_B);
    pinMode(PIN_ENCODER_PUSH, INPUT_PULLUP);
    pinMode(PIN_CONFIRM_BUTTON, INPUT_PULLUP);
    pinMode(PIN_BACK_BUTTON, INPUT_PULLUP);
    pinMode(WIFI_LED, OUTPUT);

    WiFi.begin("TP-Link_CDE6", "79222006");
    wifi.connect(5000);

    growPlant.GoToPageIntro();
    webServer.begin();

    // Task’lar
    xTaskCreate(Task_WifiLed, "WifiLed", 1024, NULL, 1, NULL);
    xTaskCreate(Task_Display, "DisplayTask", 4096, NULL, 1, NULL);
    xTaskCreate(Task_WiFiMonitor, "WiFiMonitor", 2048, NULL, 1, NULL);
    xTaskCreate([](void *)
                { 
        for (;;) { ws.cleanupClients(); vTaskDelay(pdMS_TO_TICKS(10000)); } }, "WSCleanup", 2048, NULL, 1, NULL);
}

void loop()
{
    growPlant.ChangePage(DpProtocol.ReadEncoderDetentSteps());
    growPlant.SelectedPage();
}
