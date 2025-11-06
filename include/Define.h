#ifndef DEFINE_H
#define DEFINE_H
#include <stdint.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "GrowPlant.h"
#include "MyWiFi.h"
#include "RealTimeClock.h"
#include "DisplayProtocol.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "WebServer.h"
#include <esp_task_wdt.h>
// #include "WPS.h"
// #include "esp_wifi.h"    // wifi_config_t, esp_wifi_get_config vb.
// #include "esp_err.h"     // esp_err_t
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define OLED_RESET_PIN -1
#define OLED_I2C_ADDRESS 0x3C
extern Adafruit_SSD1306 oled; // sadece referans
extern RealTimeClock rtc;     // sadece bildir
#define TOTAL_PAGES 3
#define PAGE_INTRO 0
#define PAGE_BLUETOOTH 1
#define PAGE_WIFI 2
#define WDT_TIMEOUT 4
// PIN
#define PIN_ENCODER_A 33
#define PIN_ENCODER_B 32
#define PIN_ENCODER_PUSH 25
#define PIN_CONFIRM_BUTTON 26
#define PIN_BACK_BUTTON 27
#define WIFI_LED 23
#endif // DEFINE_H
