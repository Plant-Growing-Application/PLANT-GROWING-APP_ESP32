#include "define.h"
#define FILESYSTEM LittleFS
SqlManager &sql = SqlManager::Instance();

WebServerManager::WebServerManager(AsyncWebServer &server, AsyncWebSocket &ws, MyWiFi &wifiRef)
    : _server(server), _ws(ws), _wifi(wifiRef) {}

void WebServerManager::begin()
{
    if (!FILESYSTEM.begin(false))
    {
        Serial.println("❌ LittleFS başlatılamadı!");
        return;
    }
    initWebSocket();

    // ---- HTML / API endpoints ----
    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleRoot(request); });

    _server.on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleWiFi(request); });

    _server.on("/login", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleLogin(request); });

    _server.on("/db.html", HTTP_GET, [this](AsyncWebServerRequest *request)
               { request->send(FILESYSTEM, "/db.html", "text/html"); });

    _server.on("/style.css", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleStyle(request); });

    _server.on("/bootstrap.min.css", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleBootstrap(request); });

    _server.on("/script.js", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleScript(request); });

    // WiFi kaydet
    _server.on("/saveWiFi", HTTP_POST, [this](AsyncWebServerRequest *request)
               { handleSaveWiFi(request); });

    // Tarama
    _server.on("/scan", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleScan(request); });

    _server.on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest *request)
               {
        String json = "{";

        json += "\"waterFlow\":";
        json += Sensor.WaterFlow;
        json += ",";

        json += "\"temperature\":";
        json += Sensor.WaterTemprature;
        json += ",";

        // THESE ARE FOR FUTURE USE 
        // json += "\"sensorNutrient\":";
        // json += Sensor.Pressure;
        // json += ",";

        // json += "\"sensorPH\":";
        // json += Sensor.Level;

        json += "}";

        request->send(200, "application/json", json); });
    // ---- LOGIN ----
    _server.on(
        "/api/login",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t)
        {
            String body;
            for (size_t i = 0; i < len; i++)
                body += (char)data[i];

            StaticJsonDocument<256> doc;
            if (deserializeJson(doc, body))
            {
                request->send(400, "application/json", "{\"ok\":false}");
                return;
            }

            String username = doc["username"] | "";
            String password = doc["password"] | "";

            SqlManager &sql = SqlManager::Instance();

            // 🟡 İLK KURULUM (CONFIGURED = 0)
            if (!sql.IsConfigured())
            {
                if (username == "admin" && password == "12345")
                {
                    request->send(200, "application/json",
                                  "{\"ok\":true,\"setup\":true}");
                }
                else
                {
                    request->send(401, "application/json", "{\"ok\":false}");
                }
                return;
            }

            // 🟢 NORMAL LOGIN
            if (sql.CheckUser(username, password))
            {
                request->send(200, "application/json", "{\"ok\":true}");
            }
            else
            {
                request->send(401, "application/json", "{\"ok\":false}");
            }
        });

    _server.on("/setup.html", HTTP_GET, [](AsyncWebServerRequest *request)
               {
 if (SqlManager::Instance().IsConfigured())
{
    request->redirect("/login");
    return;
}

    request->send(LittleFS, "/setup.html", "text/html"); });

    _server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request)
               {
    bool configured = SqlManager::Instance().IsConfigured();
    bool internet = WiFi.status() == WL_CONNECTED;

    String json = "{";
    json += "\"configured\":" + String(configured ? "true" : "false") + ",";
    json += "\"internet\":" + String(internet ? "true" : "false");
    json += "}";

    request->send(200, "application/json", json); });

    _server.on(
        "/api/setup-user",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t)
        {
            String body;
            for (size_t i = 0; i < len; i++)
                body += (char)data[i];

            StaticJsonDocument<256> doc;
            if (deserializeJson(doc, body))
            {
                request->send(400, "application/json", "{\"ok\":false}");
                return;
            }

            String username = doc["username"];
            String password = doc["password"];

            bool ok = sql.CreateUser(username, password);
            if (ok)
            {
                sql.SetConfigured(); // kurulum tamam
            }

            request->send(200, "application/json",
                          ok ? "{\"ok\":true}" : "{\"ok\":false}");
        });

    // THIS EVERY TIME SHOULD BE AT THE END OF BEGIN FUNCTION
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

void WebServerManager::handleBootstrap(AsyncWebServerRequest *request)
{
    request->send(FILESYSTEM, "/bootstrap.min.css", "text/css");
}

void WebServerManager::handleWiFi(AsyncWebServerRequest *request)
{
    request->send(FILESYSTEM, "/wifi.html", "text/html");
}

void WebServerManager::handleLogin(AsyncWebServerRequest *request)
{
    if (!SqlManager::Instance().IsConfigured())
    {
        request->redirect("/setup.html");
        return;
    }

    request->send(FILESYSTEM, "/login.html", "text/html");
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
    // Mevcut tarama durumunu kontrol et
    int16_t n = WiFi.scanComplete();

    if (n == -1)
    {
        // Tarama şu an devam ediyor, meşgul yanıtı dön
        request->send(202, "application/json", "{\"status\":\"scanning\"}");
    }
    else if (n == -2)
    {
        // Tarama henüz başlatılmamış, asenkron (show_hidden=false, async=true) başlat
        WiFi.scanNetworks(true, false);
        request->send(202, "application/json", "{\"status\":\"started\"}");
    }
    else
    {
        // Tarama bitti, sonuçları hazırla
        String json = "[";
        for (int i = 0; i < n; i++)
        {
            if (i > 0)
                json += ",";
            json += "{";
            json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
            json += "\"rssi\":" + String(WiFi.RSSI(i));
            json += "}";
        }
        json += "]";

        // Belleği boşalt ve yanıtı gönder
        WiFi.scanDelete();
        request->send(200, "application/json", json);
    }
}
void WebServerManager::handleNotFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "404 Not Found");
}
