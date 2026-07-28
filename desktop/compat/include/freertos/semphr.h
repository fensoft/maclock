#pragma once

#include <freertos/FreeRTOS.h>

struct LocalSemaphore;
using SemaphoreHandle_t = LocalSemaphore *;

SemaphoreHandle_t xSemaphoreCreateMutex();
BaseType_t xSemaphoreTake(
    SemaphoreHandle_t semaphore, TickType_t timeout);
BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore);
void vSemaphoreDelete(SemaphoreHandle_t semaphore);
