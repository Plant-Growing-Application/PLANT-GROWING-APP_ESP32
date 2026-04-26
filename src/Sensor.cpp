#include "Define.h"
SensorClass Sensor;
int waterTemp = 0;
int prevWaterTemp = 0;
int waterFlow = 0;
int prevWaterFlow = 0;
void SensorClass::SensorValues()
{
    waterTemp = TempratureSensor.WaterTemprature();
    waterFlow = SpeedSensor.GetWaterFlowRate();

    Sensor.WaterTemprature = waterTemp;
    Sensor.WaterFlow = waterFlow;

    if (waterFlow != prevWaterFlow)
    {
        prevWaterFlow = waterFlow;
        oled.fillRect(0, 28, 128, 10, SSD1306_BLACK);
        oled.setTextSize(1);
        oled.setCursor(0, 28);
        oled.print("WaterFlow: ");
        oled.print(waterFlow);
        oled.display();
    }
}
