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

    if (waterFlow != prevWaterFlow)
    {
        prevWaterFlow = waterFlow;
        // Ortada bilgi
        oled.fillRect(0, 28, 128, 10, SSD1306_BLACK);
        oled.setTextSize(1);
        oled.setCursor(0, 28);
        oled.print("WaterFlow: ");
        oled.print(waterFlow);
        oled.display();
    }
    if (waterTemp != prevWaterTemp)
    {
        prevWaterTemp = waterTemp;
        // Ortada bilgi
        oled.fillRect(0, 48, 128, 10, SSD1306_BLACK);
        oled.setTextSize(1);
        oled.setCursor(0, 48);
        oled.print("WaterTemp: ");
        oled.print(waterTemp);
        oled.display();
    }
}
