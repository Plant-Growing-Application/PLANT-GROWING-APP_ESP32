#include "define.h"

wifi_mode_t mode = WiFi.getMode();
GrowPlant growPlant;
DisplayProtocol DpProtocol;
MyWiFi wifi("wifi");
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WebServerManager webServer(server, ws);
RealTimeClock rtc("pool.ntp.org", 10800, 0); // GMT+3

String currentIP = "";
String currentMAC = "";
String currentTime = "";

void Task_Display(void *pvParameters)
{
    esp_task_wdt_add(NULL);
    for (;;)
    {
        growPlant.ChangePage(DpProtocol.ReadEncoderDetentSteps());
        growPlant.SelectedPage();
        vTaskDelay(pdMS_TO_TICKS(50));
        esp_task_wdt_reset();
    }
}

void Task_WiFiMonitor(void *pvParameters)
{
    esp_task_wdt_add(NULL);
    for (;;)
    {
        growPlant.ReConnectWifi();
        growPlant.SendWifiInfo();
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_task_wdt_reset();
    }
}
void Task_WifiLed(void *pvParameters)
{
    esp_task_wdt_add(NULL);
    for (;;)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            digitalWrite(WIFI_LED, HIGH);
            vTaskDelay(pdMS_TO_TICKS(200));
            digitalWrite(WIFI_LED, LOW);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        else
        {
            digitalWrite(WIFI_LED, LOW);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        esp_task_wdt_reset();
    }
}

void setup()
{
    Serial.begin(115200);
    Wire.begin(21, 22);

    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS))
    {
        Serial.println("❌ OLED başlatılamadı!");
        while (true)
            ;
    }

    growPlant.GoToPageIntro();
    DpProtocol.SetupEncoder(PIN_ENCODER_A, PIN_ENCODER_B);
    pinMode(PIN_ENCODER_PUSH, INPUT_PULLUP);
    pinMode(PIN_CONFIRM_BUTTON, INPUT_PULLUP);
    pinMode(PIN_BACK_BUTTON, INPUT_PULLUP);
    pinMode(WIFI_LED, OUTPUT);
    digitalWrite(WIFI_LED, LOW);

    WiFi.begin("TP-Link_CDE6", "79222006");
    wifi.connect(20000);

    webServer.begin();
    rtc.begin();

    xTaskCreate(Task_WiFiMonitor, "WiFiMonitor", 8192, NULL, 1, NULL);
    xTaskCreate(Task_Display, "DisplayTask", 8192, NULL, 2, NULL);
    xTaskCreate(Task_WifiLed, "WifiLed", 2048, NULL, 3, NULL);

    // WHATCH DOG TIMER
    esp_task_wdt_init(WDT_TIMEOUT, true);
    Serial.println("✅ Setup tamamlandı, task'lar başlatıldı!");
}

void loop()
{
    ws.cleanupClients();
}
