#include "Define.h"
TempratureSensorClass TempratureSensor;

int sensorValue = 0;
int temperature = 0;

int TempratureSensorClass::WaterTemprature()
{
    sensorValue = analogRead(PIN_WATER_TEMPRATURE);
    temperature = (int)(1.0 / (log((4095.0 / sensorValue) - 1.0) / 3950.0 + (1.0 / 298.15)) - 273.15);
    return temperature;
}

