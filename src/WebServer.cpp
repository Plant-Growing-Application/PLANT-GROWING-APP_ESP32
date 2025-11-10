#include "WebServer.h"
#include <LittleFS.h>
#include <WiFi.h>

#define FILESYSTEM LittleFS
#define RELAY1 16
#define RELAY2 17

WebServerManager::WebServerManager(AsyncWebServer &server, AsyncWebSocket &ws, MyWiFi &wifiRef)
    : _server(server), _ws(ws), _wifi(wifiRef) {}

void WebServerManager::begin() {
    if (!FILESYSTEM.begin(true)) {
        Serial.println("❌ LittleFS başlatılamadı!");
        return;
    }

    // Röle pinlerini ayarla
    pinMode(RELAY1, OUTPUT);
    pinMode(RELAY2, OUTPUT);
    digitalWrite(RELAY1, LOW);
    digitalWrite(RELAY2, LOW);

    initWebSocket();

    // Web sayfası ve dosya handler'ları
    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request){ handleRoot(request); });
    _server.on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request){ handleWiFi(request); });
    _server.on("/style.css", HTTP_GET, [this](AsyncWebServerRequest *request){ handleStyle(request); });
    _server.on("/script.js", HTTP_GET, [this](AsyncWebServerRequest *request){ handleScript(request); });

    // WiFi kaydetme
    _server.on("/saveWiFi", HTTP_POST, [this](AsyncWebServerRequest *request){ handleSaveWiFi(request); });

    // Tarama ve diğer REST API endpointleri
    _server.on("/scan", HTTP_GET, [this](AsyncWebServerRequest *request){ handleScan(request); });

    _server.onNotFound([this](AsyncWebServerRequest *request){ handleNotFound(request); });

    _server.begin();
    Serial.println("🌐 Web Server başlatıldı!");
}

void WebServerManager::initWebSocket() {
    _ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len){
        if(type == WS_EVT_DATA) handleWebSocketMessage(arg, data, len);
    });
    _server.addHandler(&_ws);
}

void WebServerManager::handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if(info->final && info->opcode == WS_TEXT){
        String msg = String((char*)data).substring(0, len);

        int idStart = msg.indexOf("\"id\":");
        if(idStart == -1) return;
        int idEnd = msg.indexOf("}", idStart);
        int id = msg.substring(idStart + 5, idEnd).toInt();

        int pin = (id == 1) ? RELAY1 : RELAY2;
        digitalWrite(pin, !digitalRead(pin));

        String state = digitalRead(pin) ? "ON" : "OFF";
        String json = "{\"id\":" + String(id) + ",\"state\":\"" + state + "\"}";
        _ws.textAll(json);
        _ws.cleanupClients();
    }
}

// HTML / CSS / JS handler'ları
void WebServerManager::handleRoot(AsyncWebServerRequest *request){
    request->send(FILESYSTEM, "/index.html", "text/html");
}

void WebServerManager::handleWiFi(AsyncWebServerRequest *request){
    request->send(FILESYSTEM, "/wifi.html", "text/html");
}

void WebServerManager::handleStyle(AsyncWebServerRequest *request){
    request->send(FILESYSTEM, "/style.css", "text/css");
}

void WebServerManager::handleScript(AsyncWebServerRequest *request){
    request->send(FILESYSTEM, "/script.js", "application/javascript");
}

// WiFi ayarlarını kaydet
void WebServerManager::handleSaveWiFi(AsyncWebServerRequest *request){
    if(request->hasArg("ssid") && request->hasArg("pass")){
        String ssid = request->arg("ssid");
        String pass = request->arg("pass");

        _wifi.setSSID(ssid.c_str());
        _wifi.setPassword(pass.c_str());
        _wifi.saveSettings();

        request->send(200, "text/plain", "WIFI:OK");
        delay(1000);
        ESP.restart();
    } else {
        request->send(200, "text/plain", "WIFI:FAIL");
    }
}

// WiFi taraması endpoint'i
void WebServerManager::handleScan(AsyncWebServerRequest *request){
    WiFi.mode(WIFI_MODE_APSTA);
    int n = WiFi.scanNetworks();
    String json = "[";
    for(int i=0; i<n; i++){
        if(i) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]";
    WiFi.scanDelete();
    request->send(200, "application/json", json);
}

void WebServerManager::handleNotFound(AsyncWebServerRequest *request){
    request->send(404, "text/plain", "404 Not Found");
}
