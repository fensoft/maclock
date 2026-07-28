#pragma once

#include <mutex>

struct portMUX_TYPE
{
    std::recursive_mutex mutex;
};

#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(mux) (mux)->mutex.lock()
#define portEXIT_CRITICAL(mux) (mux)->mutex.unlock()
