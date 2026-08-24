#include "alarm_ui_internal.h"

#include "audio_volume.h"
#include "sound_selector.h"

namespace
{
constexpr uint8_t kAllWeekdays = 0x7F;
constexpr uint32_t kAlarmRampStepMs = 10000;

uint8_t alarm_weekday_bit(const DateTime &now)
{
    return static_cast<uint8_t>(1U << ((now.dayOfTheWeek() + 6) % 7));
}
}

AlarmService *active_alarm_service = nullptr;

AlarmService::State &AlarmService::state()
{
    return *state_;
}

void AlarmService::begin(Preferences &preferences)
{
    if (!state_)
        state_ = new State();
    active_alarm_service = this;
    alarm_storage_begin(*this, preferences);
}

int AlarmService::due(const DateTime &now)
{
    const uint32_t now_seconds = now.unixtime();
    if (g_snooze_alarm >= 0 && now_seconds >= g_snooze_at)
    {
        const int alarm_index = g_snooze_alarm;
        g_snooze_alarm = -1;
        g_snooze_at = 0;
        return alarm_index;
    }

    const uint32_t minute_stamp = now_seconds / 60;
    const uint8_t weekday_bit = alarm_weekday_bit(now);
    int first_due_alarm = -1;
    bool one_time_changed = false;
    for (size_t i = 0; i < kAlarmCount; ++i)
    {
        AlarmSettings &alarm = g_alarms[i];
        if (!alarm.enabled || alarm.hour != now.hour() ||
            alarm.minute != now.minute() ||
            (!alarm.one_time && (alarm.weekdays & weekday_bit) == 0) ||
            g_last_trigger_minute[i] == minute_stamp)
            continue;
        g_last_trigger_minute[i] = minute_stamp;
        if (alarm.one_time)
        {
            alarm.enabled = 0;
            one_time_changed = true;
        }
        if (first_due_alarm < 0)
            first_due_alarm = static_cast<int>(i);
    }
    if (one_time_changed)
        alarm_storage_save();
    return first_due_alarm;
}

void AlarmService::snooze(size_t alarm_index, const DateTime &now)
{
    if (alarm_index >= kAlarmCount)
        return;
    g_snooze_alarm = static_cast<int>(alarm_index);
    g_snooze_at = now.unixtime() + kAlarmSnoozeSeconds;
}

void AlarmService::dismiss()
{
    g_snooze_alarm = -1;
    g_snooze_at = 0;
}

bool AlarmService::hasActiveIndicator() const
{
    if (g_snooze_alarm >= 0)
        return true;
    for (size_t i = 0; i < kAlarmCount; ++i)
        if (g_alarms[i].enabled)
            return true;
    return false;
}

bool AlarmService::upcoming(const DateTime &now, UpcomingAlarm &upcoming_alarm) const
{
    upcoming_alarm = UpcomingAlarm();
    const uint32_t now_seconds = now.unixtime();
    uint32_t best_time = UINT32_MAX;
    int best_index = -1;
    bool best_snoozed = false;
    if (g_snooze_alarm >= 0 && g_snooze_at > now_seconds)
    {
        best_time = g_snooze_at;
        best_index = g_snooze_alarm;
        best_snoozed = true;
    }
    for (size_t i = 0; i < kAlarmCount; ++i)
    {
        const AlarmSettings &alarm = g_alarms[i];
        if (!alarm.enabled)
            continue;
        for (uint8_t day_offset = 0; day_offset <= 7; ++day_offset)
        {
            const DateTime day(now_seconds + static_cast<uint32_t>(day_offset) * 86400U);
            const DateTime candidate(day.year(), day.month(), day.day(), alarm.hour, alarm.minute, 0);
            const uint32_t candidate_time = candidate.unixtime();
            if (candidate_time <= now_seconds ||
                (!alarm.one_time && (alarm.weekdays & alarm_weekday_bit(candidate)) == 0))
                continue;
            if (candidate_time < best_time)
            {
                best_time = candidate_time;
                best_index = static_cast<int>(i);
                best_snoozed = false;
            }
            break;
        }
    }
    if (best_index < 0)
        return false;
    const AlarmSettings &alarm = g_alarms[static_cast<size_t>(best_index)];
    const DateTime next(best_time);
    upcoming_alarm.valid = true;
    upcoming_alarm.snoozed = best_snoozed;
    upcoming_alarm.one_time = !best_snoozed && alarm.one_time;
    upcoming_alarm.index = static_cast<size_t>(best_index);
    upcoming_alarm.unix_time = best_time;
    const DateTime today(now.year(), now.month(), now.day(), 0, 0, 0);
    const DateTime next_day(next.year(), next.month(), next.day(), 0, 0, 0);
    upcoming_alarm.day_offset = static_cast<uint8_t>((next_day.unixtime() - today.unixtime()) / 86400);
    upcoming_alarm.weekday = static_cast<uint8_t>((next.dayOfTheWeek() + 6) % 7);
    upcoming_alarm.hour = next.hour();
    upcoming_alarm.minute = next.minute();
    strlcpy(upcoming_alarm.label, alarm.label, sizeof(upcoming_alarm.label));
    return true;
}

const char *AlarmService::soundPath(size_t alarm_index) const
{
    return SoundSelector::resolvePath(alarm_index < kAlarmCount ? g_alarm_sound_paths[alarm_index] : "/quack.mp3", "/quack.mp3");
}

uint8_t AlarmService::volume(size_t alarm_index) const
{
    return audio_volume_from_index(alarm_index < kAlarmCount ? g_alarms[alarm_index].volume : kDefaultAudioVolumeIndex);
}

uint8_t AlarmService::ringingVolume(size_t alarm_index, uint32_t elapsed_ms) const
{
    if (alarm_index >= kAlarmCount || !g_alarms[alarm_index].gradual_volume)
        return volume(alarm_index);
    size_t step = elapsed_ms / kAlarmRampStepMs;
    if (step > g_alarms[alarm_index].volume)
        step = g_alarms[alarm_index].volume;
    return audio_volume_from_index(static_cast<uint8_t>(step));
}

AlarmSettings AlarmService::settings(size_t alarm_index) const
{
    return alarm_index < kAlarmCount ? g_alarms[alarm_index] : AlarmSettings();
}

bool AlarmService::configure(size_t alarm_index, const AlarmSettings &settings, const char *sound_path)
{
    if (alarm_index >= kAlarmCount || !alarm_config_is_valid(settings) || !sound_path || !sound_path[0])
        return false;
    g_alarms[alarm_index] = settings;
    strlcpy(g_alarm_sound_paths[alarm_index], SoundSelector::resolvePath(sound_path, "/quack.mp3"), SOUND_SELECTOR_PATH_MAX);
    g_last_trigger_minute[alarm_index] = UINT32_MAX;
    if (g_snooze_alarm == static_cast<int>(alarm_index) && !settings.enabled)
        g_snooze_alarm = -1;
    alarm_storage_save();
    return true;
}
