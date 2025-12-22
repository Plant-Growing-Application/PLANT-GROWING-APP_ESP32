#include "define.h"
#include "MyEeproom.h"
#include <LittleFS.h>
#include "SqlManager.h"

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

void Task_Display(void *pvParameters)
{
    esp_task_wdt_add(NULL);
    for (;;)
    {
        GrowPlant.ChangePage(DpProtocol.ReadEncoderDetentSteps());
        GrowPlant.SelectedPage();
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
            WiFi.mode(WIFI_AP);
            WiFi.softAP("ESP32_SERVER");
            currentIP = WiFi.softAPIP().toString();
            MyEeprom.Setting.IsServerMode = true;
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
        GrowPlant.SendWifiInfo();
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
    for (;;)
    {
        if (DB.GetDB() == nullptr)
        {
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        int sensorValue = random(20, 99);
        DB.InsertSensor(sensorValue);

        Serial.print("💾 Kaydedildi: ");
        Serial.println(sensorValue);

        vTaskDelay(pdMS_TO_TICKS(10000));
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

    // 🔥 SQLITE MUTLAKA BURADA
    if (!SqlManager::Instance().Begin())
    {
        Serial.println("❌ SQLite başlatılamadı!");
    }
    else
    {
        Serial.println("✔ SQLite hazır");
    }

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
    pinMode(TEST_SENSOR_VALUE, INPUT);
    digitalWrite(WIFI_LED, LOW);

    MywiFi.attachWpsHandler(); // event bağla
    bool connected = MywiFi.connect(4000);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    if (!connected)
    {
        Serial.println("⚠️ WiFi yok → Server Mod başlatılıyor...");
        WiFi.mode(WIFI_AP);
        // SoftAP başlat (ESP kendi ağı)
        WiFi.softAP("ESP32_SERVER", "12345678");
        currentIP = WiFi.softAPIP().toString();
        Serial.println("🌐 SoftAP IP: " + currentIP);
        MyEeprom.Setting.IsServerMode = true;
    }
    else
    {
        Serial.println("✅ Kayıtlı bilgiler ile bağlandı");
        currentIP = WiFi.localIP().toString();
        MyEeprom.Setting.IsServerMode = false;
    }
    webServer.begin();
    rtc.begin();

    // Tasklar
    xTaskCreate(Task_WiFiMonitor, "WiFiMonitor", 8192, NULL, 1, NULL);
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
