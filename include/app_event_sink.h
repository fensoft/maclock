#pragma once

#include <RTClib.h>

#include "app_types.h"

class AppEventSink
{
public:
    virtual ~AppEventSink() = default;

    virtual void requestState(UiState state) = 0;
    virtual void adjustRtc(const DateTime &date_time) = 0;
    virtual void snoozeActiveAlarm() = 0;
    virtual void dismissActiveAlarm() = 0;
    virtual void dismissTimer() = 0;
};
