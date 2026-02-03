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
    if (!Execute(SQL_CREATE_SYSTEM_TABLE))
        return false;

    // 👇 TEK SATIR GARANTİ
    Execute("INSERT OR IGNORE INTO system (id, configured) VALUES (1, 0);");

    Serial.println("[SQL] ✔ System + Users hazır");
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

// ---------------- AUTH ----------------

int SqlManager::GetUserCount()
{
    const char *sql = "SELECT COUNT(*) FROM users;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    return count;
}

bool SqlManager::CheckUser(const String &username, const String &password)
{
    const char *sql =
        "SELECT id FROM users WHERE username=? AND password=?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

    bool ok = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return ok;
}

bool SqlManager::CreateUser(const String &username, const String &password)
{
    const char *sql =
        "INSERT INTO users (username, password) VALUES (?, ?);";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}
bool SqlManager::IsConfigured()
{
    const char *sql = "SELECT configured FROM system WHERE id=1;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    bool configured = false;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        configured = sqlite3_column_int(stmt, 0) == 1;

    sqlite3_finalize(stmt);
    return configured;
}

void SqlManager::SetConfigured()
{
    Execute("UPDATE system SET configured=1 WHERE id=1;");
}
