#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include "sqlite3.h"

class SqlManager
{
public:
    static SqlManager &Instance();

    bool Begin();
    bool IsReady() const { return ready && db != nullptr; }

    sqlite3 *GetDB() { return db; }

private:
    SqlManager() {}

    sqlite3 *db = nullptr;
    bool ready = false;

    const char *DB_PATH = "/system.db";

    bool CreateTables();
    bool Execute(const char *sql);

    // 👤 USER TABLE
    static constexpr const char *SQL_CREATE_USERS_TABLE =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT UNIQUE,"
        "password TEXT"
        ");";
};
