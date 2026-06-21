#include "shared_data.h"
#include "esp_now.h"
#include "esp_wifi.h"

void dashboard_task(void *pvParameters)
{
    inference_result_t result;

    while(1)
    {
        if(xQueueReceive(
            inference_queue,
            &result,
            portMAX_DELAY))
        {
            esp_now_send(
                NULL,
                (uint8_t*)&result,
                sizeof(result)
            );
        }
    }
}