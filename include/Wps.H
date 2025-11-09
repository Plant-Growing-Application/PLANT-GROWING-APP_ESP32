#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wps.h>

class WpsManager {
public:
    void begin();
    void start();
private:
    static void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
};
