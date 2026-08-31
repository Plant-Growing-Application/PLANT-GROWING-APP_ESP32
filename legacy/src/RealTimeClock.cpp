#include "define.h"

RealTimeClock::RealTimeClock(const char *ntpServer, long gmtOffset, int daylightOffset)
    : ntpServer(ntpServer), gmtOffset(gmtOffset), daylightOffset(daylightOffset), timeSynced(false)
{
}

void RealTimeClock::begin()
{
    configTime(gmtOffset, daylightOffset, ntpServer);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        timeSynced = true;
        Serial.println(getFormattedDate() + " " + getFormattedTime());
    }
}

bool RealTimeClock::updateTime()
{
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        timeSynced = true;
        return true;
    }
    return false;
}

String RealTimeClock::getFormattedTime()
{
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        char buffer[10];
        strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
        return String(buffer);
    }
    return "00:00:00";
}

String RealTimeClock::getFormattedDate()
{
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        char buffer[12];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
        return String(buffer);
    }
    return "1970-01-01";
}

int RealTimeClock::getHour()
{
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
        return timeinfo.tm_hour;
    return 0;
}

int RealTimeClock::getMinute()
{
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
        return timeinfo.tm_min;
    return 0;
}

int RealTimeClock::getSecond()
{
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
        return timeinfo.tm_sec;
    return 0;
}
