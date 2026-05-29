#include "Define.h"
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

volatile int8_t encoderStepDelta = 0;
volatile uint8_t lastEncoderState = 0;
volatile uint32_t lastInterruptTimeUs = 0;

DisplayProtocol::DisplayProtocol(uint8_t pinNumber) : pin(pinNumber), lastState(HIGH), lastDebounceTimeMs(0) {}
DisplayProtocol::DisplayProtocol() : pin(0), lastState(HIGH), lastDebounceTimeMs(0) {}
void IRAM_ATTR encoderInterruptHandler()
{
    uint32_t currentTimeUs = micros();
    // 800 yerine 250 us olarak değiştirildi
    if (currentTimeUs - lastInterruptTimeUs < 250)
        return;
    lastInterruptTimeUs = currentTimeUs;

    int pinAState = digitalRead(33);
    int pinBState = digitalRead(32);

    // Geçerli pin durumunu al
    uint8_t currentState = (pinAState << 1) | pinBState;
    uint8_t previousState = lastEncoderState;

    // Yön tespiti (Dörtlü Adımlama/Quadrature Logic)
    if (previousState == 0b00)
    {
        if (currentState == 0b01)
            encoderStepDelta++;
        else if (currentState == 0b10)
            encoderStepDelta--;
    }
    else if (previousState == 0b01)
    {
        if (currentState == 0b11)
            encoderStepDelta++;
        else if (currentState == 0b00)
            encoderStepDelta--;
    }
    else if (previousState == 0b11)
    {
        if (currentState == 0b10)
            encoderStepDelta++;
        else if (currentState == 0b01)
            encoderStepDelta--;
    }
    else if (previousState == 0b10)
    {
        if (currentState == 0b00)
            encoderStepDelta++;
        else if (currentState == 0b11)
            encoderStepDelta--;
    }

    lastEncoderState = currentState;
}
int DisplayProtocol::ReadEncoderDetentSteps()
{
    static int accumulatedSteps = 0;
    int8_t delta;
    noInterrupts();
    delta = encoderStepDelta;
    encoderStepDelta = 0;
    interrupts();
    if (!delta)
        return 0;
    accumulatedSteps += delta;
    const double stepsPerDetent = 1.5; // Bu değer enkoderin kalitesine göre 4 olabilir.
    int detentCount = 0;
    while (accumulatedSteps >= stepsPerDetent)
    {
        accumulatedSteps -= stepsPerDetent;
        detentCount++;
    }
    while (accumulatedSteps <= -stepsPerDetent)
    {
        accumulatedSteps += stepsPerDetent;
        detentCount--;
    }
    return detentCount;
}
void DisplayProtocol::SetupEncoder(uint8_t pinA, uint8_t pinB)
{
    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinB, INPUT_PULLUP);
    lastEncoderState = (digitalRead(pinA) << 1) | digitalRead(pinB);
    attachInterrupt(digitalPinToInterrupt(pinA), encoderInterruptHandler, CHANGE);
    attachInterrupt(digitalPinToInterrupt(pinB), encoderInterruptHandler, CHANGE);
}
