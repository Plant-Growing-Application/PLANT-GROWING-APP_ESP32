#include "Define.h"
SpeedSensorClass SpeedSensor;
volatile uint32_t pulseCount = 0;

void IRAM_ATTR flowISR()
{
    pulseCount++;
}
void SpeedSensorClass::SetupSpeedSensor()
{
    pinMode(PIN_WATER_FLOW, INPUT_PULLUP);
    attachInterrupt(
        digitalPinToInterrupt(PIN_WATER_FLOW),
        flowISR,
        FALLING);
}

int SpeedSensorClass::GetWaterFlowRate()
{
    noInterrupts();
    uint32_t pulses = pulseCount;
    pulseCount = 0;
    interrupts();

    // YF-S401
    // L/dk = pulses * 100 / 450
    int litersPerMinute = (pulses * 100) / 450;

    return litersPerMinute;
}