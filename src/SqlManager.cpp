#include "SqlManager.h"
#include <LittleFS.h>

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
    // Mutlaka önce LittleFS açılmalı
    if (!LittleFS.begin(true))
    {
        Serial.println("❌ LittleFS mount FAILED!");
        return false;
    }

    // Doğru SQLite dosya yolu
    String path = "/littlefs/";
    path += dbName; // örn: sensor.db
    const char *finalPath = path.c_str();

    Serial.print("📂 SQLite açılıyor: ");
    Serial.println(finalPath);

    int rc = sqlite3_open(finalPath, &Db);

    if (rc != SQLITE_OK)
    {
        Serial.print("❌ SQLite açılamadı! Kod: ");
        Serial.println(rc);
        Serial.print("❌ Mesaj: ");
        Serial.println(sqlite3_errmsg(Db));

        if (Db)
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

    Serial.println("✔ SQL OK");
    return true;
}
