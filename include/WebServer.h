#ifndef WEBSEVER_MANEGER_H
#define WEBSEVER_MANEGER_H

#pragma once
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "MyWiFi.h"

class WebServerManager
{
public:
    WebServerManager(AsyncWebServer &server, AsyncWebSocket &ws, MyWiFi &wifiRef);
    void begin();

private:
    AsyncWebServer &_server;
    AsyncWebSocket &_ws;
    MyWiFi &_wifi;

    void initWebSocket();
    void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
    void handleDBRows(AsyncWebServerRequest *request);
    void handleRoot(AsyncWebServerRequest *request);
    void handleWiFi(AsyncWebServerRequest *request);
    void handleStyle(AsyncWebServerRequest *request);
    void handleScript(AsyncWebServerRequest *request);
    void handleSaveWiFi(AsyncWebServerRequest *request);
    void handleScan(AsyncWebServerRequest *request);
    void handleNotFound(AsyncWebServerRequest *request);
    void clearSensorTable();
};
extern WebServerManager WebServer;

#endif /* WEBSEVER_MANEGER_H */
