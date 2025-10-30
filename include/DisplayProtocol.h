#ifndef DISPLAY_PROTOCOL_H
#define DISPLAY_PROTOCOL_H

class DisplayProtocol
{
private:
public:
    uint8_t pin;
    bool lastState;
    uint32_t lastDebounceTimeMs;
    DisplayProtocol(uint8_t pinNumber);
    DisplayProtocol();
    void SetupEncoder(uint8_t pinA, uint8_t pinB);
    int ReadEncoderDetentSteps();
};
extern DisplayProtocol DpProtocol;

#endif // DISPLAY_PROTOCOL_H