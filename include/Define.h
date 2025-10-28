#ifndef DEFINE_H
#define DEFINE_H
#include <stdint.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "App_Controller.h"
#include "Display.h"
#include "Controls.h"
#include "Pins.h"
#include "DisplayProtocol.h"
#include "MyWiFi.h"
extern Adafruit_SSD1306 oled; // sadece referans

#define TOTAL_PAGES 3
#define PAGE_INTRO 0
#define PAGE_BLUETOOTH 1
#define PAGE_WIFI 2

// PIN
#define PIN_ENCODER_A 33
#define PIN_ENCODER_B 32
#define PIN_ENCODER_PUSH 25
#define PIN_CONFIRM_BUTTON 26
#define PIN_BACK_BUTTON 27
#endif // DEFINE_H
