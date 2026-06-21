#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
    float x;
    float y;
    float z;
} sensor_data_t;

typedef struct {
    float anomaly_score;
    int machine_fault;
} inference_result_t;

extern QueueHandle_t sensor_queue;
extern QueueHandle_t inference_queue;

#endif