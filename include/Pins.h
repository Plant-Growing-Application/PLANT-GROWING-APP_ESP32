#pragma once
#include <Arduino.h>

// -------- OLED/I2C --------
#define I2C_SDA      21
#define I2C_SCL      22
#define OLED_ADDR  0x3C
#define OLED_W     128
#define OLED_H      64

// -------- PINLER (resimdeki gibi) --------
#define ENCODER_A_PIN      32
#define ENCODER_B_PIN      33
#define ENCODER_PUSH_PIN   25
#define CONFIRM_BUTTON_PIN 26
#define BACK_BUTTON_PIN    27

// -------- Encoder ayarı --------
// Çoğu encoder 1 detent için 4 kenar üretir; fazlaysa 2 yap.
#define EDGES_PER_DETENT    2

// -------- Uygulama --------
#define APP_PAGE_COUNT      3   