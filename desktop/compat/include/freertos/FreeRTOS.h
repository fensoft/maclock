#pragma once

#include <stddef.h>
#include <stdint.h>

using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = uint32_t;
using EventBits_t = uint32_t;

#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(value) static_cast<TickType_t>(value)
