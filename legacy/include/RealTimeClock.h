#ifndef REALTIME_CLOCK_H
#define REALTIME_CLOCK_H

#include <Arduino.h>
#include <WiFi.h>
#include "time.h"

class RealTimeClock
{
public:
    RealTimeClock(const char *ntpServer = "pool.ntp.org", long gmtOffset = 10800, int daylightOffset = 0);
    void begin();              // NTP'den zamanı alır
    bool updateTime();         // Zamanı yeniden senkronize eder
    String getFormattedTime(); // "HH:MM:SS" formatında zaman döner
    String getFormattedDate(); // "YYYY-MM-DD" formatında tarih döner
    int getHour();
    int getMinute();
    int getSecond();

private:
    const char *ntpServer;
    long gmtOffset;
    int daylightOffset;
    bool timeSynced;
};
extern RealTimeClock rtc; // sadece bildir

#endif
