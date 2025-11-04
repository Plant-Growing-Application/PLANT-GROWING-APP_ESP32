#pragma once
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SPIFFS.h>

class WebServerManager
{
public:
    WebServerManager(AsyncWebServer &server, AsyncWebSocket &ws);
    void begin();

private:
    AsyncWebServer &_server;
    AsyncWebSocket &_ws;

    void initWebSocket();
    void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
};
