#include "shared_data.h"
#include "freertos/task.h"
#include "driver/i2c.h"

QueueHandle_t sensor_queue;

void sensor_task(void *pvParameters)
{
    sensor_data_t data;

    while(1)
    {
        // Read MPU6050 / ADXL345

        data.x = 0.25;
        data.y = 0.12;
        data.z = 1.01;

        xQueueSend(sensor_queue,&data,portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}