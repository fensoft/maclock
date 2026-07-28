#pragma once

#include <stddef.h>
#include <stdint.h>

using i2s_chan_handle_t = void *;

int i2s_channel_write(
    i2s_chan_handle_t handle,
    const void *source,
    size_t bytes,
    size_t *bytes_written,
    uint32_t timeout);
