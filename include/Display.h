#pragma once
#include <Arduino.h>

// Ekranı başlat (I2C + SSD1306). true -> OK, false -> hata
bool displayBegin();

// Verilen sayfayı çizer (0..N-1). (Default sayfa çizimi)
void displayDrawPage(int page);

// replace=false: Toast zaten aktifse gelen istek YOK SAYILIR.
// replace=true : Mevcut toast derhâl yeni mesaja/süreye GÜNCELLENİR.
void displayShowToast(const char *msg, uint16_t ms, bool replace = false);

// Her loop turunda çağrılmalı: süre dolunca menüye döndürür.
void displayTick(int currentPage);

void displayDrawPage1_Setpoint(int setpoint, bool edit); // 0..100
void displayDrawPage2_Volume(int volume, bool edit);     // 0..10
void displayDrawPage3_Mode(const char *mode, int idx, int count, bool edit);
