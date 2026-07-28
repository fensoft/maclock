#pragma once

#include <freertos/FreeRTOS.h>

struct LocalStreamBuffer;
using StreamBufferHandle_t = LocalStreamBuffer *;

struct StaticStreamBuffer_t
{
    LocalStreamBuffer *instance = nullptr;
};

StreamBufferHandle_t xStreamBufferCreateStatic(
    size_t capacity,
    size_t trigger_level,
    uint8_t *storage,
    StaticStreamBuffer_t *control);
size_t xStreamBufferSend(
    StreamBufferHandle_t stream,
    const void *data,
    size_t bytes,
    TickType_t timeout);
size_t xStreamBufferReceive(
    StreamBufferHandle_t stream,
    void *data,
    size_t bytes,
    TickType_t timeout);
size_t xStreamBufferBytesAvailable(
    StreamBufferHandle_t stream);
BaseType_t xStreamBufferReset(StreamBufferHandle_t stream);
