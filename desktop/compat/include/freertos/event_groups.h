#pragma once

#include <freertos/FreeRTOS.h>

struct LocalEventGroup;
using EventGroupHandle_t = LocalEventGroup *;

EventGroupHandle_t xEventGroupCreate();
EventBits_t xEventGroupSetBits(
    EventGroupHandle_t group, EventBits_t bits);
EventBits_t xEventGroupWaitBits(
    EventGroupHandle_t group,
    EventBits_t bits,
    BaseType_t clear_on_exit,
    BaseType_t wait_for_all,
    TickType_t timeout);
void vEventGroupDelete(EventGroupHandle_t group);
