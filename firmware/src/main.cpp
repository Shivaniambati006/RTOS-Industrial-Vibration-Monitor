#include <Arduino.h>

#include "sensor_task.h"
#include "inference_task.h"
#include "dashboard_task.h"
#include "alert_task.h"

void setup()
{
    Serial.begin(115200);

    xTaskCreate(
        sensorTask,
        "Sensor Task",
        4096,
        NULL,
        2,
        NULL);

    xTaskCreate(
        inferenceTask,
        "Inference Task",
        4096,
        NULL,
        2,
        NULL);

    xTaskCreate(
        dashboardTask,
        "Dashboard Task",
        4096,
        NULL,
        1,
        NULL);

    xTaskCreate(
        alertTask,
        "Alert Task",
        2048,
        NULL,
        3,
        NULL);
}

void loop()
{
}