#pragma once

#include <stddef.h>
#include <stdint.h>

#include <Preferences.h>
#include <RTClib.h>
#include <lvgl.h>

#include "audio_volume.h"
#include "app_event_sink.h"

static constexpr size_t kAlarmCount = 3;
static constexpr uint32_t kAlarmSnoozeSeconds = 9 * 60;
static constexpr size_t kAlarmLabelMaxLength = 24;

struct AlarmSettings
{
    uint8_t enabled = 0;
    uint8_t hour = 7;
    uint8_t minute = 0;
    uint8_t weekdays = 0x7F;
    uint8_t sound = 0;
    uint8_t volume = kDefaultAudioVolumeIndex;
    uint8_t one_time = 0;
    uint8_t gradual_volume = 0;
    uint8_t sunrise = 0;
    char label[kAlarmLabelMaxLength + 1] = "";
};

struct UpcomingAlarm
{
    bool valid = false;
    bool snoozed = false;
    bool one_time = false;
    size_t index = 0;
    uint32_t unix_time = 0;
    uint8_t day_offset = 0;
    uint8_t weekday = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    char label[kAlarmLabelMaxLength + 1] = "";
};

class AlarmService
{
public:
    struct State;

    void begin(Preferences &preferences);
    int due(const DateTime &now);
    void snooze(size_t alarm_index, const DateTime &now);
    void dismiss();
    bool hasActiveIndicator() const;
    bool upcoming(
        const DateTime &now, UpcomingAlarm &upcoming_alarm) const;
    const char *soundPath(size_t alarm_index) const;
    uint8_t volume(size_t alarm_index) const;
    uint8_t ringingVolume(
        size_t alarm_index, uint32_t elapsed_ms) const;
    AlarmSettings settings(size_t alarm_index) const;
    bool configure(
        size_t alarm_index,
        const AlarmSettings &settings,
        const char *sound_path);

    State &state();

private:
    State *state_ = nullptr;
};

class AlarmView
{
public:
    void begin(lv_obj_t *screen, AppEventSink &events);
    void hide();
    void enter(const DateTime &now);
    void showEditor();
    void showRinging(size_t alarm_index);
    void updateRinging(
        size_t alarm_index, uint32_t elapsed_ms);
    void refreshLanguage();
};
