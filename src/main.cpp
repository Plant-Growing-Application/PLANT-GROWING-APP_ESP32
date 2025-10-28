#include "define.h"
DisplayProtocol DisplayControl; // nesne oluştur
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
    DisplayControl.GoToPageIntro();

    pinMode(PIN_ENCODER_PUSH, INPUT_PULLUP);

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
    DisplayControl.EncoderControl(readEncoderDetentSteps());
}