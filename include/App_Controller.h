#pragma once

// Uygulamayı başlatır (Serial, input, ui)
void appBegin();

// Her döngüde çağrılır: encoder/buton olaylarını işler, UI'yi günceller
void appLoop();
void controlsBegin();
int controlsReadEncoderDetent();
bool controlsButtonFell(int button);
