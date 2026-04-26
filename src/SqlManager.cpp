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
    if (!Execute(SQL_CREATE_USERS_TABLE))
        return false;

    Serial.println("[SQL] ✔ Users hazır");
    return true;
}

bool SqlManager::Execute(const char *sql)
{
    char *err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);

    if (rc != SQLITE_OK)
    {
        Serial.printf("[SQL] ❌ SQL hata: %s\n", err);
        sqlite3_free(err);
        return false;
    }
    return true;
}
