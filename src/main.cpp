#include "define.h"

GrowPlant growPlant;        // nesne oluştur
DisplayProtocol DpProtocol; // nesne oluştur
MyWiFi wifi;

bool lastButtonState = HIGH;
bool buttonPressed = false;
RealTimeClock rtc("pool.ntp.org", 10800, 0); // GMT+3 için 10800 saniye offset

void Task_Display(void *pvParameters)
{
    for (;;)
    {
        if (growPlant.CurrentPage == PAGE_INTRO)
        {
            growPlant.ShowClock();
            growPlant.ShowIP();
            growPlant.ShowMac();
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // 50ms aralıkla kontrol
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

    // Task oluştur
    xTaskCreate(
        Task_Display,  // Task fonksiyonu
        "DisplayTask", // Task adı
        2048,          // Stack boyutu
        NULL,          // Parametre yok
        1,             // Öncelik
        NULL           // Handle
    );
}

void loop()
{
    // ONLY WORKING THE IF CHANGE THE ENCODER VALUE
    growPlant.ChangePage(DpProtocol.ReadEncoderDetentSteps());

    // ITS WORK WHEN PUSHED THE ENCODER BUTTON
    growPlant.SelectedPage();
}