#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include "sqlite3.h"

class SqlManager
{
public:
    // Singleton erişim
    static SqlManager &Instance();

    // Yaşam döngüsü
    bool Begin();        // DB aç + tabloları oluştur
    bool IsReady() const // DB hazır mı?
    {
        return ready && db != nullptr;
    }
    bool ClearSensors();

    // İş mantığı
    bool InsertSensor(float value);
    String GetAllSensorsJson();

    sqlite3 *GetDB() { return db; }

private:
    // Constructor private (singleton)
    SqlManager() {}

    // ---- Core ----
    sqlite3 *db = nullptr;
    bool ready = false;

    // ---- Config ----
    const char *DB_PATH = "/littlefs/system.db";

    // ---- Init ----
    bool CreateTables();
    bool Execute(const char *sql);

    // ---- SQL ----
    static constexpr const char *SQL_CREATE_SENSOR_TABLE =
        "CREATE TABLE IF NOT EXISTS sensor ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "value REAL NOT NULL,"
        "created INTEGER"
        ");";
};
