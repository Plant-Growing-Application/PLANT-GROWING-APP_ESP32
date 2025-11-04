#include "WebServer.h"
#include <LittleFS.h>

#define FILESYSTEM LittleFS // ✅ sadece burayı kullan

#define RELAY1 16
#define RELAY2 17

WebServerManager::WebServerManager(AsyncWebServer &server, AsyncWebSocket &ws)
    : _server(server), _ws(ws) {}

void WebServerManager::begin()
{
    if (!FILESYSTEM.begin(true))
    {
        Serial.println("❌ LittleFS başlatılamadı!");
        return;
    }

    pinMode(RELAY1, OUTPUT);
    pinMode(RELAY2, OUTPUT);
    digitalWrite(RELAY1, LOW);
    digitalWrite(RELAY2, LOW);

    initWebSocket();

    _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(FILESYSTEM, "/index.html", "text/html"); }); // ✅ LittleFS kullan

    _server.serveStatic("/", FILESYSTEM, "/"); // ✅ burası da aynı şekilde

    _server.begin();
    Serial.println("🌐 Web Server başlatıldı!");
}

void WebServerManager::initWebSocket()
{
    _ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len)
                {
        if (type == WS_EVT_DATA) handleWebSocketMessage(arg, data, len); });
    _server.addHandler(&_ws);
}

void WebServerManager::handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->opcode == WS_TEXT)
    {
        String msg = String((char *)data).substring(0, len);

        int idStart = msg.indexOf("\"id\":");
        if (idStart == -1)
            return;
        int idEnd = msg.indexOf("}", idStart);
        int id = msg.substring(idStart + 5, idEnd).toInt();
        int pin = (id == 1) ? RELAY1 : RELAY2;
        digitalWrite(pin, !digitalRead(pin));

        String state = digitalRead(pin) ? "ON" : "OFF";
        String json = "{\"id\":" + String(id) + ",\"state\":\"" + state + "\"}";
        _ws.textAll(json);
        _ws.cleanupClients(); // ✅ fazladan textAll silindi
    }
}
