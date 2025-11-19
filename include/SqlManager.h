#pragma once

#include "sqlite3.h"
#include <Arduino.h>

class SqlManager
{
public:
    static SqlManager &Instance();

    bool Begin(const char *dbPath);
    bool CreateTable();

private:
    SqlManager() = default;
    ~SqlManager();

    bool Execute(const char *sql);

    sqlite3 *Db = nullptr;
    bool isReady = false;
};
