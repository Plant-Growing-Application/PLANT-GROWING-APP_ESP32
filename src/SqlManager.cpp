#include "SqlManager.h"

SqlManager &SqlManager::Instance()
{
    static SqlManager instance;
    return instance;
}
bool SqlManager::Begin()
{
    int rc = sqlite3_open_v2(
        DB_PATH,
        &db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
        nullptr);

    if (rc != SQLITE_OK)
    {
        Serial.printf("[SQL] ❌ DB açılamadı: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        db = nullptr;
        return false;
    }

    Serial.println("[SQL] ✔ DB açıldı");

    ready = CreateTables();
    return ready;
}

bool SqlManager::CreateTables()
{
    if (!db)
        return false;

    char *errMsg = nullptr;
    int rc = sqlite3_exec(db, SQL_CREATE_SENSOR_TABLE, nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK)
    {
        Serial.printf("[SQL] ❌ Tablo hatası: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    Serial.println("[SQL] ✔ Tablolar hazır");
    return true;
}

bool SqlManager::Execute(const char *sql)
{
    char *errorMsg = nullptr;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errorMsg);

    if (rc != SQLITE_OK)
    {
        Serial.printf("[SQL] ❌ SQL hata: %s\n", errorMsg);
        sqlite3_free(errorMsg);
        return false;
    }
    return true;
}

bool SqlManager::InsertSensor(float value)
{
    if (!IsReady())
        return false;

    String sql = "INSERT INTO sensor (value) VALUES (";
    sql += String(value, 2);
    sql += ");";

    return Execute(sql.c_str());
}
String SqlManager::GetAllSensorsJson()
{
    if (!db)
    {
        Serial.println("[SQL] DB açık değil");
        return "[]";
    }
    const char *sql =
        "SELECT id, value, created "
        "FROM sensor "
        "ORDER BY id DESC;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        Serial.print("[SQL] ❌ SELECT prepare hatası: ");
        Serial.println(sqlite3_errmsg(db));
        return "[]";
    }

    String json = "[";
    bool first = true;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first)
            json += ",";
        first = false;

        json += "{";
        json += "\"id\":" + String(sqlite3_column_int(stmt, 0)) + ",";
        json += "\"value\":" + String(sqlite3_column_double(stmt, 1)) + ",";
        json += "\"created\":" + String(sqlite3_column_int(stmt, 2));
        json += "}";
    }

    json += "]";
    sqlite3_finalize(stmt);

    return json;
}
bool SqlManager::ClearSensors()
{
    if (!db)
    {
        Serial.println("[SQL] ❌ DB açık değil");
        return false;
    }

    const char *sql = "DELETE FROM sensor;";
    if (!Execute(sql))
    {
        Serial.println("[SQL] ❌ Sensor verileri silinemedi");
        return false;
    }

    Serial.println("[SQL] 🧹 Sensor verileri temizlendi");
    return true;
}
