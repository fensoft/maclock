#pragma once

#include <stddef.h>
#include <stdint.h>

#include <Preferences.h>
#include <RTClib.h>
#include <lvgl.h>

static constexpr size_t kAlarmCount = 3;
static constexpr uint32_t kAlarmSnoozeSeconds = 9 * 60;

void alarms_init(Preferences &preferences);
int alarms_due(const DateTime &now);
void alarms_snooze(size_t alarm_index, const DateTime &now);
void alarms_dismiss();
bool alarms_have_active_indicator();
const char *alarms_sound_path(size_t alarm_index);
uint8_t alarms_volume(size_t alarm_index);

void alarm_ui_init(lv_obj_t *screen);
void alarm_ui_hide();
void alarm_ui_enter();
void alarm_ui_show_editor();
void alarm_ui_show_ringing(size_t alarm_index);
