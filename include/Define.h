#ifndef DEFINE_H
#define DEFINE_H
#include <stdint.h>
#include <WiFi.h>
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "GrowPlant.h"
#include "MyWiFi.h"
#include "RealTimeClock.h"
#include "DisplayProtocol.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <EEPROM.h>
#include "MyEeproom.h"
#include "Wps.h"
#include <esp_wifi.h>
#include <esp_wps.h>
#include "esp_err.h"
#include <sqlite3.h>
#include <LittleFS.h>
#include "WebServer.h"
#include "Sensor.h"
#include "TempratureSensor.h"
#include "SpeedSensor.h"
#include "SqlManager.h"
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define OLED_RESET_PIN -1
#define OLED_I2C_ADDRESS 0x3C
extern Adafruit_SSD1306 oled; // sadece referans
extern RealTimeClock rtc;     // sadece bildir
#define TOTAL_PAGES 5
#define PAGE_INTRO 0
#define PAGE_BLUETOOTH 1
#define PAGE_WIFI 2
#define PAGE_WPS 3
#define PAGE_SENSORS 4
#define WDT_TIMEOUT 15
// PIN
#define PIN_ENCODER_A 33
#define PIN_ENCODER_B 32
#define PIN_ENCODER_PUSH 25
#define PIN_CONFIRM_BUTTON 26
#define PIN_BACK_BUTTON 27
#define READ_INTERVAL 10000
#define WIFI_LED 23
#define RELAY1 16
#define RELAY2 17
#define PIN_WATER_TEMPRATURE 35
#define PIN_WATER_FLOW 34
#endif // DEFINE_H