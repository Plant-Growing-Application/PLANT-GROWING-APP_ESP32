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
    if (currentTimeUs - lastInterruptTimeUs < 800)
        return;
    lastInterruptTimeUs = currentTimeUs;
    int pinAState = digitalRead(33);
    int pinBState = digitalRead(32);
    uint8_t currentState = (pinAState << 1) | pinBState;
    uint8_t previousState = lastEncoderState;
    if ((previousState == 0b00 && currentState == 0b01) ||
        (previousState == 0b01 && currentState == 0b11) ||
        (previousState == 0b11 && currentState == 0b10) ||
        (previousState == 0b10 && currentState == 0b00))
        encoderStepDelta++;
    else if ((previousState == 0b00 && currentState == 0b10) ||
             (previousState == 0b10 && currentState == 0b11) ||
             (previousState == 0b11 && currentState == 0b01) ||
             (previousState == 0b01 && currentState == 0b00))
        encoderStepDelta--;
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
    const int stepsPerDetent = 2;
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
