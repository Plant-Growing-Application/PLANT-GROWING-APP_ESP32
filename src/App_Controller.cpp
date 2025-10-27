// App_Controller.cpp
#include "App_Controller.h"
#include "Display.h"
#include "Controls.h"
#include "Pins.h"

enum Page : int8_t
{
    PAGE_SETPOINT,
    PAGE_VOLUME,
    PAGE_MODE,
    PAGE_BRIGHTNESS,
    PAGE_TOTAL
};

static int8_t currentPage = PAGE_SETPOINT;
static bool isEditMode = false;

int setpoint = 50, tempSetpoint = 50;
int volume = 5, tempVolume = 5;
int mode = 0, tempMode = 0;
const char *modeLabels[3] = {"AUTO", "MAN", "OFF"};
int brightness = 100;
int tempBrightness = 100;

Button encoderButton(ENCODER_PUSH);
Button confirmButton(CONFIRM_BUTTON);
Button backButton(BACK_BUTTON);

void initializeApp()
{
    Serial.begin(115200);
    initializeDisplay();
    setupEncoder(ENCODER_A, ENCODER_B);
    pinMode(ENCODER_PUSH, INPUT_PULLUP);
    pinMode(CONFIRM_BUTTON, INPUT_PULLUP);
    pinMode(BACK_BUTTON, INPUT_PULLUP);
    renderPage(currentPage, isEditMode, setpoint, tempSetpoint, volume, tempVolume, mode, tempMode, modeLabels);
}

void runAppLoop()
{
    int encoderStep = readEncoderDetentSteps();
    if (encoderStep)
    {
        if (isEditMode)
        {
            switch (currentPage)
            {
            case PAGE_SETPOINT:
                tempSetpoint = constrain(tempSetpoint + encoderStep, 0, 100);
                break;
            case PAGE_VOLUME:
                tempVolume = constrain(tempVolume + encoderStep, 0, 10);
                break;
            case PAGE_MODE:
                tempMode += encoderStep;
                if (tempMode < 0)
                    tempMode = 2;
                if (tempMode > 2)
                    tempMode = 0;
                break;
            case PAGE_BRIGHTNESS:
                tempBrightness = constrain(tempBrightness + encoderStep, 0, 255);
                break;
            }
        }
        else
        {
            currentPage += (encoderStep > 0) ? 1 : -1;
            if (currentPage < 0)
                currentPage = PAGE_TOTAL - 1;
            if (currentPage >= PAGE_TOTAL)
                currentPage = 0;
            tempSetpoint = setpoint;
            tempVolume = volume;
            tempMode = mode;
        }
        renderPage(currentPage, isEditMode, setpoint, tempSetpoint, volume, tempVolume, mode, tempMode, modeLabels);
    }

    if (isButtonPressed(encoderButton))
    {
        isEditMode = !isEditMode;
        renderPage(currentPage, isEditMode, setpoint, tempSetpoint, volume, tempVolume, mode, tempMode, modeLabels);
    }

    if (isButtonPressed(confirmButton) && isEditMode)
    {
        switch (currentPage)
        {
        case PAGE_SETPOINT:
            setpoint = tempSetpoint;
            break;
        case PAGE_VOLUME:
            volume = tempVolume;
            break;
        case PAGE_MODE:
            mode = tempMode;
            break;
        case PAGE_BRIGHTNESS:
            brightness = tempBrightness;
            break;
        }
        showMessage("Kaydedildi", 1000, currentPage); // örnek: 1 saniye göster
        renderPage(currentPage, isEditMode, setpoint, tempSetpoint, volume, tempVolume, mode, tempMode, modeLabels);
    }

    if (isButtonPressed(backButton) && isEditMode)
    {
        tempSetpoint = setpoint;
        tempVolume = volume;
        tempMode = mode;
        tempBrightness = brightness;
        showMessage("Iptal", 1000, currentPage);
        isEditMode = false;
        renderPage(currentPage, isEditMode, setpoint, tempSetpoint, volume, tempVolume, mode, tempMode, modeLabels);
    }
}