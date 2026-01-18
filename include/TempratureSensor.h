#ifndef TEMPRATURE_SENSOR_H
#define TEMPRATURE_SENSOR_H

class TempratureSensorClass
{
private:
public:
    int WaterTemprature();
    int PhSensor();
    int NutrimentSensor();
};
extern TempratureSensorClass TempratureSensor;

#endif // TEMPRATURE_SENSOR_H