#include "define.h"
#ifndef DISPLAY_H
#define DISPLAY_H


void initializeDisplay();
void renderPage(int currentPage, bool isEditMode, int setpoint, int tempSetpoint,
                int volume, int tempVolume, int mode, int tempMode, const char *modeLabels[]);
void showMessage(const char *message, uint16_t durationMs = 1000, int currentPage = 0);

#endif