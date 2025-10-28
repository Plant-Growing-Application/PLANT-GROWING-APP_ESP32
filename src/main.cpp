#include "define.h"
DisplayProtocol DisplayControl; // nesne oluştur
MyWiFi wifi;

bool lastButtonState = HIGH;
bool buttonPressed = false;
// Task fonksiyonu
// Task fonksiyonu
void Task_Display(void *pvParameters)
{
    for (;;)
    {
        DisplayControl.SelectedPage();
        vTaskDelay(pdMS_TO_TICKS(50)); // 50ms aralıkla kontrol
    }
}
void setup()
{
    initializeApp();
    pinMode(PIN_ENCODER_PUSH, INPUT_PULLUP);

    WiFi.begin("TP-Link_CDE6", "79222006");
    if (wifi.connect(10000))
    {
        Serial.println("WiFi bağlı ");
        Serial.println(wifi.getLocalIPString());
        DisplayControl.GoToPageIntro();
    }

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
    // Fonksiyonu çağırmanın doğru yolu
    // runAppLoop();
    DisplayControl.ChangePage(readEncoderDetentSteps());
}