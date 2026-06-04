#include "inference_task.h"

void inferenceTask(void *parameter)
{
    while(true)
    {
        // Run Edge Impulse

        Serial.println("Running Inference");

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}