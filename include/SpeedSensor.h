#ifndef SPEED_SENSOR_H
#define SPEED_SENSOR_H

class SpeedSensorClass
{
private:
public:
    int GetWaterFlowRate();
    void SetupSpeedSensor();
};
extern SpeedSensorClass SpeedSensor;

#endif // SPEED_SENSOR_H