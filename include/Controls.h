// Controls.h
#ifndef CONTROLS_H
#define CONTROLS_H

#include <Arduino.h>

class Button
{
public:
    uint8_t pin;
    bool lastState;
    uint32_t lastDebounceTimeMs;

    Button(uint8_t pinNumber);
    Button();
};

void setupEncoder(uint8_t pinA, uint8_t pinB);
int readEncoderDetentSteps();
bool isButtonPressed(Button &btn);

#endif