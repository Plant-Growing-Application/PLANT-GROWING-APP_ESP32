#include "define.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

SqlManager &DB = SqlManager::Instance();
DisplayProtocol DpProtocol;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WebServerManager webServer(server, ws, MywiFi); // 3. parametre eklendi
RealTimeClock rtc("pool.ntp.org", 10800, 0);    // GMT+3
unsigned long lastSave = 0;

String currentIP = "";
String currentMAC = "";
String currentTime = "";
unsigned long nowTime = 0;
unsigned long previousTime = 0;

// Global handle for WiFiMonitor task
TaskHandle_t xWiFiMonitorHandle = NULL;

// Function to set WiFi mode and state
void setWiFiMode(wifi_mode_t mode, bool isServerMode) {
    WiFi.mode(mode);
    MyEeprom.Setting.IsServerMode = isServerMode;
    MyEeprom.SaveSettings(MyEeprom.Setting);
    // Start softAP if in AP or AP_STA mode
    if (mode == WIFI_AP || mode == WIFI_AP_STA) {
        WiFi.softAP("ESP32_SERVER", "12345678");
    }
    // Note: For WIFI_STA mode, the softAP is turned off by the WiFi.mode() call.
}

// Function to pause and resume WiFiMonitor task
void pauseWiFiMonitor() {
    if (xWiFiMonitorHandle != NULL) {
        vTaskSuspend(xWiFiMonitorHandle);
    }
}

void resumeWiFiMonitor() {
    if (xWiFiMonitorHandle != NULL) {
        vTaskResume(xWiFiMonitorHandle);
    }
}

void Task_Display(void *pvParameters)
{
    esp_task_wdt_add(NULL);
    static unsigned long lastSensorPageRefresh = 0;

    for (;;)
    {
        GrowPlant.ChangePage(DpProtocol.ReadEncoderDetentSteps());
        GrowPlant.SelectedPage();

        if (GrowPlant.CurrentPage == PAGE_SENSORS)
        {
            unsigned long now = millis();
            if (now - lastSensorPageRefresh >= 500)
            {
                lastSensorPageRefresh = now;
                Sensor.SensorValues();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
        esp_task_wdt_reset();
    }
}

void Task_WiFiMonitor(void *pvParameters)
{
    esp_task_wdt_add(NULL);

    if (!MywiFi.isConnected() && !MyEeprom.Setting.IsServerMode)
    {
        bool connected = MywiFi.connect(5000);
        if (!connected)
        {
            Serial.println("⚠️ WiFi bağlanamadı, Server Mod aktif");
            setWiFiMode(WIFI_AP, true);
            currentIP = WiFi.softAPIP().toString();
        }
        else
        {
            currentIP = WiFi.localIP().toString();
        }
    }

    for (;;)
    {
        // sadece bağlantı kontrolü
        if (!MyEeprom.Setting.IsServerMode && !MywiFi.isConnected())
            MywiFi.connect(5000);
        MywiFi.ConnectFromWPS();
        MywiFi.ConnectFromWeb();
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
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
void Task_SensorLogger(void *pvParameters)
{
    static unsigned long lastSensorUpdate = 0;
    static unsigned long lastDbLog = 0;

    for (;;)
    {
        nowTime = millis();

        if (nowTime - lastSensorUpdate >= 600)
        {
            lastSensorUpdate = nowTime;
            Sensor.WaterFlow = SpeedSensor.GetWaterFlowRate();
            Sensor.WaterTemprature = TempratureSensor.WaterTemprature();
        }

        if (DB.IsReady() && nowTime - lastDbLog >= 10000)
        {
            lastDbLog = nowTime;
            float tempValue = TempratureSensor.WaterTemprature();
            Serial.print("💾 Kaydedildi: ");
            Serial.println(tempValue);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void setup()
{
    Serial.begin(115200);
    Wire.begin(21, 22);
    MyEeprom.Begin();
    // 🔥 LittleFS
    if (!LittleFS.begin(true))
    {
        Serial.println("❌ LittleFS mount failed");
        return;
    }
    Serial.println("✔ LittleFS hazır");

    // EEPROM’dan ayarları oku
    if (!MyEeprom.GetSettings(MyEeprom.Setting))
    {
        Serial.println("EEPROM boş veya geçersiz, varsayılan ayarlar kullanılıyor.");
        memset(&MyEeprom.Setting, 0, sizeof(Settings));
    }
    Serial.print("Connecting to SSID: ");
    Serial.println(MyEeprom.Setting.SSID);
    Serial.print("Password: ");
    Serial.println(MyEeprom.Setting.Password);
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS))
    {
        Serial.println("❌ OLED başlatılamadı!");
        while (true)
            ;
    }

    GrowPlant.GoToPageIntro();
    DpProtocol.SetupEncoder(PIN_ENCODER_A, PIN_ENCODER_B);
    pinMode(PIN_ENCODER_PUSH, INPUT_PULLUP);
    pinMode(PIN_CONFIRM_BUTTON, INPUT_PULLUP);
    pinMode(PIN_BACK_BUTTON, INPUT);
    pinMode(WIFI_LED, OUTPUT);
    pinMode(PIN_WATER_TEMPRATURE, INPUT);
    digitalWrite(WIFI_LED, LOW);
    // Röle pinleri
    pinMode(RELAY1, OUTPUT);
    pinMode(RELAY2, OUTPUT);
    digitalWrite(RELAY1, LOW);
    digitalWrite(RELAY2, LOW);

    WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info)
                  { MywiFi.onWiFiEvent(event); });
    bool connected = MywiFi.connect(4000);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    if (!connected)
    {
        Serial.println("⚠️ WiFi yok → Server Mod başlatılıyor...");
        setWiFiMode(WIFI_AP, true);
        currentIP = WiFi.softAPIP().toString();
        Serial.println("🌐 SoftAP IP: " + currentIP);
    }
    else
    {
        Serial.println("✅ Kayıtlı bilgiler ile bağlandı");
        setWiFiMode(WIFI_STA, false);
        currentIP = WiFi.localIP().toString();
    }
    SpeedSensor.SetupSpeedSensor();
    webServer.begin();
    rtc.begin();

    // Tasklar
    xTaskCreate(Task_WiFiMonitor, "WiFiMonitor", 8192, NULL, 1, &xWiFiMonitorHandle);
    xTaskCreate(Task_Display, "DisplayTask", 4096, NULL, 2, NULL);
    xTaskCreate(Task_WifiLed, "WifiLed", 2048, NULL, 3, NULL);
    xTaskCreate(Task_SensorLogger, "SensorLogger", 4096, NULL, 4, NULL);

    esp_task_wdt_init(WDT_TIMEOUT, true);
    Serial.println("✅ Setup tamamlandı!");
    GrowPlant.SendWifiInfo();
}

void loop()
{
    ws.cleanupClients();
    // GrowPlant.TestPins();
}
