#include "shared_data.h"
#include "driver/gpio.h"

#define ALERT_LED GPIO_NUM_2

void alert_task(void *pvParameters)
{
    inference_result_t result;

    gpio_set_direction(
        ALERT_LED,
        GPIO_MODE_OUTPUT
    );

    while(1)
    {
        if(xQueueReceive(
            inference_queue,
            &result,
            portMAX_DELAY))
        {
            if(result.machine_fault)
            {
                gpio_set_level(
                    ALERT_LED,
                    1
                );
            }
            else
            {
                gpio_set_level(
                    ALERT_LED,
                    0
                );
            }
        }
    }
}