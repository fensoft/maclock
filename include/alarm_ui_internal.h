#pragma once

#include "alarm_ui.h"
#include "sound_selector.h"

#include <string.h>

enum AlarmEditorPage
{
    ALARM_PAGE_HOME,
    ALARM_PAGE_SELECT,
    ALARM_PAGE_TIME,
    ALARM_PAGE_DAYS,
    ALARM_PAGE_OPTIONS,
    ALARM_PAGE_LABEL,
    ALARM_PAGE_SOUND,
    ALARM_PAGE_VOLUME,
    ALARM_PAGE_ACTIONS,
    ALARM_PAGE_COUNT
};

struct AlarmEditorUi
{
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *pages[ALARM_PAGE_COUNT];
    lv_obj_t *slot_matrix;
    lv_obj_t *enabled_matrix;
    lv_obj_t *time_value;
    lv_obj_t *time_matrix;
    lv_obj_t *days_matrix;
    lv_obj_t *options_matrix;
    lv_obj_t *label_value;
    lv_obj_t *label_matrix;
    SoundSelector sound_selector;
    lv_obj_t *volume_matrix;
    lv_obj_t *summary;
    lv_obj_t *previous;
    lv_obj_t *previous_label;
    lv_obj_t *exit;
    lv_obj_t *exit_label;
    lv_obj_t *next;
    lv_obj_t *next_label;
    lv_obj_t *home_alarm_label;
    lv_obj_t *home_timer_label;
    lv_obj_t *upcoming;
    lv_obj_t *save_label;
};

struct AlarmRingingUi
{
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *time;
    lv_obj_t *sound;
    lv_obj_t *snooze_label;
    lv_obj_t *dismiss_label;
};

struct AlarmService::State
{
    Preferences *preferences = nullptr;
    AlarmSettings alarms[kAlarmCount] = {};
    AlarmSettings edit_alarms[kAlarmCount] = {};
    char alarm_sound_paths[kAlarmCount][SOUND_SELECTOR_PATH_MAX] = {};
    char edit_alarm_sound_paths[kAlarmCount][SOUND_SELECTOR_PATH_MAX] = {};
    uint32_t last_trigger_minute[kAlarmCount] = {};
    int snooze_alarm = -1;
    uint32_t snooze_at = 0;
    size_t selected_alarm = 0;
    AlarmEditorPage editor_page = ALARM_PAGE_HOME;
    AlarmEditorUi editor = {};
    AlarmRingingUi ringing = {};
    AppEventSink *events = nullptr;
    const char *slot_map[4] = {};
    const char *enabled_map[3] = {};
    const char *time_map[5] = {};
    const char *days_map[9] = {};
    const char *options_map[6] = {};
    const char *label_map[7] = {};
    const char *page_names[ALARM_PAGE_COUNT] = {};
};

extern AlarmService *active_alarm_service;

void alarm_storage_begin(AlarmService &service, Preferences &preferences);
void alarm_storage_save();
bool alarm_config_is_valid(const AlarmSettings &alarm);
void alarm_editor_begin(lv_obj_t *screen, AppEventSink &events);
void alarm_editor_hide();
void alarm_editor_enter(const DateTime &now);
void alarm_editor_show();
void alarm_editor_refresh_language();
void alarm_ringing_begin(lv_obj_t *screen);
void alarm_ringing_hide();
void alarm_ringing_show(size_t alarm_index);
void alarm_ringing_update(size_t alarm_index, uint32_t elapsed_ms);
void alarm_ringing_refresh_language();

#define g_preferences (active_alarm_service->state().preferences)
#define g_alarms (active_alarm_service->state().alarms)
#define g_edit_alarms (active_alarm_service->state().edit_alarms)
#define g_alarm_sound_paths (active_alarm_service->state().alarm_sound_paths)
#define g_edit_alarm_sound_paths (active_alarm_service->state().edit_alarm_sound_paths)
#define g_last_trigger_minute (active_alarm_service->state().last_trigger_minute)
#define g_snooze_alarm (active_alarm_service->state().snooze_alarm)
#define g_snooze_at (active_alarm_service->state().snooze_at)
#define g_selected_alarm (active_alarm_service->state().selected_alarm)
#define g_editor_page (active_alarm_service->state().editor_page)
#define g_editor (active_alarm_service->state().editor)
#define g_ringing (active_alarm_service->state().ringing)
#define g_events (active_alarm_service->state().events)
#define g_slot_map (active_alarm_service->state().slot_map)
#define g_enabled_map (active_alarm_service->state().enabled_map)
#define g_time_map (active_alarm_service->state().time_map)
#define g_days_map (active_alarm_service->state().days_map)
#define g_options_map (active_alarm_service->state().options_map)
#define g_label_map (active_alarm_service->state().label_map)
#define g_page_names (active_alarm_service->state().page_names)
