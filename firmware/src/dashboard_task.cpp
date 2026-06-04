#include "dashboard_task.h"

void dashboardTask(void *parameter)
{
    while(true)
    {
        Serial.println("Updating Dashboard");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}