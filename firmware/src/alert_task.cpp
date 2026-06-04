#include "alert_task.h"

void alertTask(void *parameter)
{
    while(true)
    {
        Serial.println("Checking Alerts");

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}