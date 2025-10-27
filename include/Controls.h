
#pragma once
#include <Arduino.h>

enum class Button { EncoderPush, Confirm, Back };

// Başlatma: pinMode + encoder interrupt’ları
void inputBegin();

// Encoder’dan son çağrıdan bu yana kaç detent (±) okundu
int  inputReadEncoderDetent();

// Buton için HIGH->LOW (basıldı) geçişini bir kez döndürür
bool inputButtonFell(ButtonId b);
