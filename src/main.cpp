#include "define.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "WebServer.h"

GrowPlant growPlant;        // nesne oluştur
DisplayProtocol DpProtocol; // nesne oluştur
MyWiFi wifi;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WebServerManager webServer(server, ws);

bool lastButtonState = HIGH;
bool buttonPressed = false;
RealTimeClock rtc("pool.ntp.org", 10800, 0); // GMT+3 offset

void Task_Display(void *pvParameters)
{
    for (;;)
    {
        if (growPlant.CurrentPage == PAGE_INTRO && WiFi.status() == WL_CONNECTED)
        {
            growPlant.ShowClock();
            growPlant.ShowIP();
            growPlant.ShowMac();
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // 50ms aralıkla kontrol
    }
}
void Task_WebSocketCleanup(void *pvParameters)
{
    for (;;)
    {
        ws.cleanupClients();
        vTaskDelay(pdMS_TO_TICKS(10000)); // her 10 saniyede bir
    }
}
void setup()
{
    Serial.begin(115200);
    Wire.begin(21, 22);

    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS))
    {
        while (true)
            ; // sonsuz döngü
    }
    DpProtocol.SetupEncoder(PIN_ENCODER_A, PIN_ENCODER_B);
    pinMode(PIN_ENCODER_PUSH, INPUT_PULLUP);
    pinMode(PIN_CONFIRM_BUTTON, INPUT_PULLUP);
    pinMode(PIN_BACK_BUTTON, INPUT_PULLUP);
    pinMode(PIN_ENCODER_PUSH, INPUT_PULLUP);

    WiFi.begin("TP-Link_CDE6", "79222006");
    if (wifi.connect(5000))
    {
        growPlant.ShowIP();
        growPlant.ShowMac();
    }
    growPlant.GoToPageIntro();

    rtc.begin();

    webServer.begin(); // 🌐 Web arayüzünü başlat
    // Task oluştur
    xTaskCreate(
        Task_Display,  // Task fonksiyonu
        "DisplayTask", // Task adı
        2048,          // Stack boyutu
        NULL,          // Parametre yok
        1,             // Öncelik
        NULL           // Handle
    );
    xTaskCreate(Task_WebSocketCleanup, "WSCleanup", 2048, NULL, 1, NULL);
}

void loop()
{
    // ONLY WORKING THE IF CHANGE THE ENCODER VALUE
    growPlant.ChangePage(DpProtocol.ReadEncoderDetentSteps());

    // ITS WORK WHEN PUSHED THE ENCODER BUTTON
    growPlant.SelectedPage();
}