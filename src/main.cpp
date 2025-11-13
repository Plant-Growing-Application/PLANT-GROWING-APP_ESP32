#include "define.h"
GrowPlant growPlant;
DisplayProtocol DpProtocol;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WebServerManager webServer(server, ws, wifi); // 3. parametre eklendi
RealTimeClock rtc("pool.ntp.org", 10800, 0);  // GMT+3

String currentIP = "";
String currentMAC = "";
String currentTime = "";
bool serverModeActive = false;

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

    if (!wifi.isConnected() && !serverModeActive)
    {
        bool connected = wifi.connect(5000);
        if (!connected)
        {
            Serial.println("⚠️ WiFi bağlanamadı, Server Mod aktif");
            WiFi.mode(WIFI_AP);
            WiFi.softAP("ESP32_SERVER");
            currentIP = WiFi.softAPIP().toString();
            serverModeActive = true; // artık tekrar açılmaz
        }
        else
        {
            currentIP = WiFi.localIP().toString();
        }
    }

    for (;;)
    {
        // sadece bağlantı kontrolü
        if (!serverModeActive && !wifi.isConnected())
        {
            wifi.connect(5000);
        }

        growPlant.SendWifiInfo();
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

void setup()
{
    Serial.begin(115200);
    Wire.begin(21, 22);
    MyEeprom.Begin();
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

    growPlant.GoToPageIntro();
    DpProtocol.SetupEncoder(PIN_ENCODER_A, PIN_ENCODER_B);
    pinMode(PIN_ENCODER_PUSH, INPUT_PULLUP);
    pinMode(PIN_CONFIRM_BUTTON, INPUT_PULLUP);
    pinMode(PIN_BACK_BUTTON, INPUT_PULLUP);
    pinMode(WIFI_LED, OUTPUT);
    digitalWrite(WIFI_LED, LOW);

    wifi.attachWpsHandler(); // event bağla
    if (strlen(MyEeprom.Setting.SSID) == 0)
    {
        Serial.println("EEPROM boş, AP mod başlatılıyor");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("MyDeviceAP");
    }
    else
    {
        WiFi.mode(WIFI_STA);
        WiFi.begin(MyEeprom.Setting.SSID, MyEeprom.Setting.Password);
    }
    bool connected = wifi.connect(20000);

    if (!connected)
    {
        Serial.println("⚠️ WiFi yok → Server Mod başlatılıyor...");
        WiFi.mode(WIFI_AP);
        // SoftAP başlat (ESP kendi ağı)
        WiFi.softAP("ESP32_SERVER", "12345678");
        currentIP = WiFi.softAPIP().toString();
        Serial.println("🌐 SoftAP IP: " + currentIP);
    }
    else
    {
        Serial.println("✅ Kayıtlı bilgiler ile bağlandı");
        currentIP = WiFi.localIP().toString();
    }

    webServer.begin();
    rtc.begin();

    xTaskCreate(Task_WiFiMonitor, "WiFiMonitor", 8192, NULL, 1, NULL);
    xTaskCreate(Task_Display, "DisplayTask", 4096, NULL, 2, NULL);
    xTaskCreate(Task_WifiLed, "WifiLed", 2048, NULL, 3, NULL);

    esp_task_wdt_init(WDT_TIMEOUT, true);
    Serial.println("✅ Setup tamamlandı!");
}

void loop()
{
    ws.cleanupClients();

    if (webServer.wifiShouldReconnect)
    {
        webServer.wifiShouldReconnect = false;

        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.begin(MyEeprom.Setting.SSID, MyEeprom.Setting.Password);

        Serial.print("Connecting to SSID: ");
        Serial.println(MyEeprom.Setting.SSID);

        // Opsiyonel: WiFi bağlanana kadar bekle
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
        {
            delay(100); // küçük delay → WDT tetiklemez
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.print("Connected! IP: ");
            Serial.println(WiFi.localIP());
        }
        else
        {
            Serial.println("Failed to connect.");
        }
    }
}
