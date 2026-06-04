#include "sensor_task.h"

void sensorTask(void *parameter)
{
    while(true)
    {
        // Read MPU6050

        Serial.println("Reading Sensor");

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}