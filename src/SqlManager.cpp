#include "SqlManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

SqlManager &SqlManager::Instance()
{
    static SqlManager instance;
    return instance;
}

SqlManager::~SqlManager()
{
    if (Db)
    {
        sqlite3_close(Db);
        Db = nullptr;
    }
}

bool SqlManager::Begin(const char *dbName)
{
    if (!LittleFS.begin(true))
    {
        Serial.println("❌ LittleFS mount FAILED!");
        return false;
    }

    String path = "/littlefs/";
    path += dbName;
    const char *finalPath = path.c_str();

    Serial.print("📂 SQLite açılıyor: ");
    Serial.println(finalPath);

    int rc = sqlite3_open(finalPath, &Db);

    if (rc != SQLITE_OK)
    {
        Serial.println("❌ SQLite açılamadı!");
        Serial.println(sqlite3_errmsg(Db));

        sqlite3_close(Db);
        Db = nullptr;

        return false;
    }

    Serial.println("✅ SQLite bağlantısı açıldı");
    isReady = true;
    
    return CreateTable();
}

bool SqlManager::CreateTable()
{
    const char *sql =
        "CREATE TABLE IF NOT EXISTS sensor ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "value REAL,"
        "time TEXT DEFAULT CURRENT_TIMESTAMP"
        ");";

    return Execute(sql);
}

bool SqlManager::Execute(const char *sql)
{
    char *errMsg = nullptr;
    int rc = sqlite3_exec(Db, sql, nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK)
    {
        Serial.print("❌ SQL error: ");
        Serial.println(errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

// ⭐ SENSÖR DEĞERİ EKLEME
bool SqlManager::InsertSensorValue(float value)
{
    if (!Db) return false;

    const char *sql = "INSERT INTO sensor (value) VALUES (?);";
    sqlite3_stmt *stmt = nullptr;

    if (sqlite3_prepare_v2(Db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        Serial.println(sqlite3_errmsg(Db));
        return false;
    }

    sqlite3_bind_double(stmt, 1, value);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        Serial.println(sqlite3_errmsg(Db));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

// ⭐ TÜM SATIRLARI JSON FORMATINDA GETİR
String SqlManager::GetAllRowsAsJson()
{
    DynamicJsonDocument doc(8192);
    JsonArray arr = doc.to<JsonArray>();

    const char *sql = "SELECT id, value, time FROM sensor ORDER BY id DESC LIMIT 50;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(Db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            JsonObject row = arr.createNestedObject();
            row["id"] = sqlite3_column_int(stmt, 0);
            row["value"] = sqlite3_column_double(stmt, 1);
            row["time"] = (const char *)sqlite3_column_text(stmt, 2);
        }
    }

    sqlite3_finalize(stmt);

    String json;
    serializeJson(arr, json);
    return json;
}
