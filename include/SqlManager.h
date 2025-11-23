#ifndef SQLMANAGER_H
#define SQLMANAGER_H

#include <Arduino.h>
#include <sqlite3.h>

class SqlManager
{
public:
    static SqlManager &Instance();
    ~SqlManager();

    bool Begin(const char *dbName);
    bool CreateTable();
    bool Execute(const char *sql);

    bool InsertSensorValue(float value);
    String GetAllRowsAsJson();

    sqlite3 *Db = nullptr;
    bool isReady = false;

private:
    SqlManager() {}
};

#endif
