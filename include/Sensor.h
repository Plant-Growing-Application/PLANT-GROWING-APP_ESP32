#ifndef SENSOR_H
#define SENSOR_H

class SensorClass
{
private:
public:
    void SensorValues();
    int WaterFlow = 0;
    int WaterTemprature = 0;
};
extern SensorClass Sensor;

#endif // SENSOR_H