#pragma once

#include <stddef.h>
#include <stdint.h>

#include <Preferences.h>
#include <RTClib.h>
#include <lvgl.h>

#include "app_event_sink.h"

static constexpr size_t kAlarmCount = 3;
static constexpr uint32_t kAlarmSnoozeSeconds = 9 * 60;

class AlarmService
{
public:
    struct State;

    void begin(Preferences &preferences);
    int due(const DateTime &now);
    void snooze(size_t alarm_index, const DateTime &now);
    void dismiss();
    bool hasActiveIndicator() const;
    const char *soundPath(size_t alarm_index) const;
    uint8_t volume(size_t alarm_index) const;

    State &state();

private:
    State *state_ = nullptr;
};

class AlarmView
{
public:
    void begin(lv_obj_t *screen, AppEventSink &events);
    void hide();
    void enter();
    void showEditor();
    void showRinging(size_t alarm_index);
    void refreshLanguage();
};
