#include "shared_data.h"

QueueHandle_t sensor_queue;
QueueHandle_t inference_queue;

extern void sensor_task(void *);
extern void inference_task(void *);
extern void alert_task(void *);
extern void dashboard_task(void *);

void app_main()
{
    sensor_queue =
        xQueueCreate(
            20,
            sizeof(sensor_data_t)
        );

    inference_queue =
        xQueueCreate(
            20,
            sizeof(inference_result_t)
        );

    xTaskCreate(
        sensor_task,
        "sensor_task",
        4096,
        NULL,
        5,
        NULL
    );

    xTaskCreate(
        inference_task,
        "inference_task",
        8192,
        NULL,
        6,
        NULL
    );

    xTaskCreate(
        alert_task,
        "alert_task",
        4096,
        NULL,
        4,
        NULL
    );

    xTaskCreate(
        dashboard_task,
        "dashboard_task",
        4096,
        NULL,
        4,
        NULL
    );
}