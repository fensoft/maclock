#pragma once

#include <freertos/FreeRTOS.h>

struct LocalTask;
using TaskHandle_t = LocalTask *;
using TaskFunction_t = void (*)(void *);

BaseType_t xTaskCreatePinnedToCore(
    TaskFunction_t function,
    const char *name,
    uint32_t stack_depth,
    void *parameter,
    UBaseType_t priority,
    TaskHandle_t *created_task,
    BaseType_t core);
void vTaskDelay(TickType_t ticks);
void vTaskDelete(TaskHandle_t task);
void vTaskSuspend(TaskHandle_t task);
void vTaskResume(TaskHandle_t task);

void maclock_local_freertos_shutdown();
