#include "shared_data.h"

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

QueueHandle_t inference_queue;

void inference_task(void *pvParameters)
{
    sensor_data_t sensor;

    inference_result_t result;

    while(1)
    {
        if(xQueueReceive(sensor_queue,&sensor,portMAX_DELAY))
        {
            signal_t signal;

            // Fill Edge Impulse input buffer

            ei_impulse_result_t ei_result;

            run_classifier(
                &signal,
                &ei_result,
                false
            );

            result.anomaly_score =
                ei_result.anomaly;

            result.machine_fault =
                result.anomaly_score > 0.7;

            xQueueSend(
                inference_queue,
                &result,
                portMAX_DELAY
            );
        }
    }
}