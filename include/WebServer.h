// WebServer.h
#pragma once
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "MyWiFi.h"

class WebServerManager
{
public:
    WebServerManager(AsyncWebServer &server, AsyncWebSocket &ws, MyWiFi &wifiRef);

    void begin();
    bool wifiShouldReconnect;

private:
    AsyncWebServer &_server;
    AsyncWebSocket &_ws;
    MyWiFi &_wifi;

    void initWebSocket();
    void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);

    void handleRoot(AsyncWebServerRequest *request);
    void handleWiFi(AsyncWebServerRequest *request);
    void handleStyle(AsyncWebServerRequest *request);
    void handleScript(AsyncWebServerRequest *request);
    void handleSaveWiFi(AsyncWebServerRequest *request);
    void handleScan(AsyncWebServerRequest *request);
    void handleNotFound(AsyncWebServerRequest *request);
};
