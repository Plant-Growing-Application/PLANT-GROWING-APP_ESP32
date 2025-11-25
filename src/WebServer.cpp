#include "define.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <SQLite3.h>

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

    _server.on("/api/get-rows", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleDBRows(request); });

    // ⭐ SENSOR VERILERINI SIFIRLAMA API ⭐
    _server.on("/api/clear-sensor", HTTP_POST, [this](AsyncWebServerRequest *request)
               {
                   clearSensorTable();
                   request->send(200, "text/plain", "CLEARED"); });

    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleRoot(request); });

    _server.on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleWiFi(request); });

    _server.on("/style.css", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleStyle(request); });

    _server.on("/script.js", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleScript(request); });

    // DB indir
    _server.on("/download_db", HTTP_GET, [](AsyncWebServerRequest *request)
               {
        if (LittleFS.exists("/sensor.db")) {
            request->send(LittleFS, "/sensor.db", "application/octet-stream");
        } else {
            request->send(404, "text/plain", "DB Not Found");
        } });

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

        _ws.textAll(json);
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

        MywiFi.wifiShouldReconnect = true;

        request->send(200, "text/plain", "WIFI:OK");
        return;
    }

    request->send(200, "text/plain", "WIFI:FAIL");
}

// ---- SQLITE TABLO OKUMA ----
void WebServerManager::handleDBRows(AsyncWebServerRequest *request)
{
    sqlite3 *db;
    sqlite3_open("/littlefs/sensor.db", &db);

    const char *sql = "SELECT id, sensor, value, time FROM logs;";
    sqlite3_stmt *stmt;

    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            JsonObject row = arr.createNestedObject();
            row["id"] = sqlite3_column_int(stmt, 0);
            row["sensor"] = (const char *)sqlite3_column_text(stmt, 1);
            row["value"] = sqlite3_column_double(stmt, 2);
            row["time"] = (const char *)sqlite3_column_text(stmt, 3);
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    String json;
    serializeJson(arr, json);

    request->send(200, "application/json", json);
}

// ---- SQLITE TABLO TEMİZLEME ⭐⭐ ----
void WebServerManager::clearSensorTable()
{
    SqlManager::Instance().ClearTable(); // Tek noktadan temizleme
}

// ---- WIFI TARAMA ----
void WebServerManager::handleScan(AsyncWebServerRequest *request)
{
    WiFi.mode(WIFI_MODE_AP);
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
