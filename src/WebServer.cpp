#include "define.h"
#include <LittleFS.h>
#include <WiFi.h>

#define FILESYSTEM LittleFS

WebServerManager::WebServerManager(AsyncWebServer &server, AsyncWebSocket &ws, MyWiFi &wifiRef)
    : _server(server), _ws(ws), _wifi(wifiRef) {}

void WebServerManager::begin()
{
    if (!FILESYSTEM.begin(true))
    {
        Serial.println("❌ LittleFS başlatılamadı!");
        return;
    }

    // Röle pinleri
    pinMode(RELAY1, OUTPUT);
    pinMode(RELAY2, OUTPUT);
    digitalWrite(RELAY1, LOW);
    digitalWrite(RELAY2, LOW);

    initWebSocket();

    // ---- HTML / API endpoints ----

    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleRoot(request); });

    _server.on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleWiFi(request); });

    _server.on("/style.css", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleStyle(request); });

    _server.on("/script.js", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleScript(request); });

    // WiFi kaydet
    _server.on("/saveWiFi", HTTP_POST, [this](AsyncWebServerRequest *request)
               { handleSaveWiFi(request); });

    // Tarama
    _server.on("/scan", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleScan(request); });

    _server.onNotFound([this](AsyncWebServerRequest *request)
                       { handleNotFound(request); });

    _server.begin();
    Serial.println("🌐 Web Server başladı!");
}

void WebServerManager::initWebSocket()
{
    _ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len)
                {
        if (type == WS_EVT_DATA)
            handleWebSocketMessage(arg, data, len); });

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
        bool newState = !digitalRead(pin);
        digitalWrite(pin, newState);

        String json = "{\"id\":" + String(id) + ",\"state\":\"" +
                      (newState ? "ON" : "OFF") + "\"}";

        AsyncWebSocketMessageBuffer *buffer = _ws.makeBuffer(json.length());
        memcpy(buffer->get(), json.c_str(), json.length());
        _ws.textAll(buffer);
    }
}

// ---- HTML HANDLERS ----

void WebServerManager::handleRoot(AsyncWebServerRequest *request)
{
    request->send(FILESYSTEM, "/index.html", "text/html");
}

void WebServerManager::handleWiFi(AsyncWebServerRequest *request)
{
    request->send(FILESYSTEM, "/wifi.html", "text/html");
}

void WebServerManager::handleStyle(AsyncWebServerRequest *request)
{
    request->send(FILESYSTEM, "/style.css", "text/css");
}

void WebServerManager::handleScript(AsyncWebServerRequest *request)
{
    request->send(FILESYSTEM, "/script.js", "application/javascript");
}

// ---- WIFI KAYDET ----
void WebServerManager::handleSaveWiFi(AsyncWebServerRequest *request)
{
    if (request->hasArg("ssid") && request->hasArg("pass"))
    {
        String ssid = request->arg("ssid");
        String pass = request->arg("pass");

        memset(MyEeprom.Setting.SSID, 0, sizeof(MyEeprom.Setting.SSID));
        strncpy(MyEeprom.Setting.SSID, ssid.c_str(), sizeof(MyEeprom.Setting.SSID) - 1);

        memset(MyEeprom.Setting.Password, 0, sizeof(MyEeprom.Setting.Password));
        strncpy(MyEeprom.Setting.Password, pass.c_str(), sizeof(MyEeprom.Setting.Password) - 1);

        MyEeprom.Setting.IsWpsActive = false;
        MyEeprom.SaveSettings(MyEeprom.Setting);

        _wifi.wifiShouldReconnect = true;

        request->send(200, "text/plain", "WIFI:OK");
        return;
    }

    request->send(200, "text/plain", "WIFI:FAIL");
}
// ---- WIFI TARAMA ----
void WebServerManager::handleScan(AsyncWebServerRequest *request)
{
    int n = WiFi.scanNetworks(false, true);

    String json = "[";
    for (int i = 0; i < n; i++)
    {
        if (i)
            json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) +
                "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]";

    WiFi.scanDelete();
    request->send(200, "application/json", json);
}

void WebServerManager::handleNotFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "404 Not Found");
}
