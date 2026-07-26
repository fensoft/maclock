#include "alarm_ui.h"

#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(lv_font_chicago_8);
LV_FONT_DECLARE(lv_font_chicago_48);

extern void request_normal_state();
extern void alarm_snooze_current();
extern void alarm_dismiss_current();

namespace
{
static constexpr uint32_t kAlarmStorageMagic = 0x414C524D; // 'ALRM'
static constexpr uint8_t kAlarmStorageVersion = 1;
static constexpr uint8_t kAllWeekdays = 0x7F;
static constexpr size_t kAlarmSoundCount = 3;
static constexpr size_t kAlarmVolumeCount = 4;

struct AlarmConfig
{
    uint8_t enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t weekdays;
    uint8_t sound;
    uint8_t volume;
};

struct AlarmStorage
{
    uint32_t magic;
    uint8_t version;
    AlarmConfig alarms[kAlarmCount];
};

struct AlarmEditorUi
{
    lv_obj_t *panel;
    lv_obj_t *slot_matrix;
    lv_obj_t *hour;
    lv_obj_t *minute;
    lv_obj_t *active_time;
    lv_obj_t *enabled;
    lv_obj_t *days_matrix;
    lv_obj_t *sound_matrix;
    lv_obj_t *volume_matrix;
};

struct AlarmRingingUi
{
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *time;
    lv_obj_t *sound;
};

static Preferences *g_preferences = nullptr;
static AlarmConfig g_alarms[kAlarmCount] = {};
static AlarmConfig g_edit_alarms[kAlarmCount] = {};
static uint32_t g_last_trigger_minute[kAlarmCount] = {};
static int g_snooze_alarm = -1;
static uint32_t g_snooze_at = 0;
static size_t g_selected_alarm = 0;
static AlarmEditorUi g_editor = {};
static AlarmRingingUi g_ringing = {};

static const char *g_slot_map[] = {"1", "2", "3", ""};
static const char *g_days_map[] = {"M", "T", "W", "T", "F", "S", "S", ""};
static const char *g_sound_map[] = {"Quack", "Startup", "Floppy", ""};
static const char *g_volume_map[] = {"25", "50", "75", "100", ""};
static const char *g_sound_paths[kAlarmSoundCount] = {
    "/quack.mp3",
    "/startup.mp3",
    "/floppy.mp3"};
static const uint8_t g_volume_values[kAlarmVolumeCount] = {25, 50, 75, 100};

static void SetAlarmDefaults()
{
    memset(g_alarms, 0, sizeof(g_alarms));
    for (size_t i = 0; i < kAlarmCount; ++i)
    {
        g_alarms[i].hour = (uint8_t)(7 + i);
        g_alarms[i].weekdays = kAllWeekdays;
        g_alarms[i].sound = 0;
        g_alarms[i].volume = 2;
        g_last_trigger_minute[i] = UINT32_MAX;
    }
}

static bool AlarmConfigIsValid(const AlarmConfig &alarm)
{
    return alarm.enabled <= 1 &&
           alarm.hour < 24 &&
           alarm.minute < 60 &&
           (alarm.weekdays & ~kAllWeekdays) == 0 &&
           alarm.sound < kAlarmSoundCount &&
           alarm.volume < kAlarmVolumeCount;
}

static void SaveAlarms()
{
    if (!g_preferences)
        return;

    AlarmStorage storage = {};
    storage.magic = kAlarmStorageMagic;
    storage.version = kAlarmStorageVersion;
    memcpy(storage.alarms, g_alarms, sizeof(g_alarms));
    g_preferences->putBytes("alarms_v1", &storage, sizeof(storage));
}

static uint8_t AlarmWeekdayBit(const DateTime &now)
{
    const uint8_t sunday_based = now.dayOfTheWeek();
    const uint8_t monday_based = (uint8_t)((sunday_based + 6) % 7);
    return (uint8_t)(1U << monday_based);
}

static void SetMatrixChecked(lv_obj_t *matrix,
                             size_t button_count,
                             size_t checked)
{
    lv_buttonmatrix_clear_button_ctrl_all(
        matrix, LV_BUTTONMATRIX_CTRL_CHECKED);
    if (checked < button_count)
    {
        lv_buttonmatrix_set_button_ctrl(
            matrix,
            (uint32_t)checked,
            LV_BUTTONMATRIX_CTRL_CHECKED);
        lv_buttonmatrix_set_selected_button(matrix, (uint32_t)checked);
    }
}

static void StyleMatrix(lv_obj_t *matrix)
{
    const lv_style_selector_t checked_items =
        (lv_style_selector_t)LV_PART_ITEMS |
        (lv_style_selector_t)LV_STATE_CHECKED;

    lv_obj_set_style_bg_color(matrix, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(matrix, lv_color_black(), 0);
    lv_obj_set_style_border_width(matrix, 1, 0);
    lv_obj_set_style_radius(matrix, 0, 0);
    lv_obj_set_style_pad_all(matrix, 1, 0);
    lv_obj_set_style_pad_column(matrix, 1, 0);
    lv_obj_set_style_text_font(matrix, &lv_font_chicago_8, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_border_color(matrix, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(matrix, 1, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, lv_color_black(), checked_items);
    lv_obj_set_style_text_color(matrix, lv_color_white(), checked_items);
}

static void ButtonVisualEvent(lv_event_t *event)
{
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(event);
    if (!label)
        return;

    lv_obj_set_style_text_color(
        label,
        lv_event_get_code(event) == LV_EVENT_PRESSED
            ? lv_color_white()
            : lv_color_black(),
        0);
}

static lv_obj_t *CreateButton(lv_obj_t *parent,
                              const char *text,
                              lv_event_cb_t callback)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_style_bg_color(button, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(button, lv_color_black(), 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_radius(button, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_black(), LV_STATE_PRESSED);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(
        button, ButtonVisualEvent, LV_EVENT_PRESSED, label);
    lv_obj_add_event_cb(
        button, ButtonVisualEvent, LV_EVENT_RELEASED, label);
    lv_obj_add_event_cb(
        button, ButtonVisualEvent, LV_EVENT_PRESS_LOST, label);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
    return button;
}

static lv_obj_t *CreateTimeSpinbox(lv_obj_t *parent,
                                   int maximum,
                                   lv_event_cb_t callback)
{
    lv_obj_t *spinbox = lv_spinbox_create(parent);
    lv_obj_remove_style_all(spinbox);
    lv_spinbox_set_range(spinbox, 0, maximum);
    lv_spinbox_set_digit_format(spinbox, 2, 0);
    lv_spinbox_set_rollover(spinbox, true);
    lv_spinbox_set_step(spinbox, 1);
    lv_spinbox_set_cursor_pos(spinbox, 1);
    lv_obj_set_size(spinbox, 48, 27);
    lv_obj_set_style_bg_color(spinbox, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(spinbox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(spinbox, lv_color_black(), 0);
    lv_obj_set_style_border_width(spinbox, 1, 0);
    lv_obj_set_style_radius(spinbox, 0, 0);
    lv_obj_set_style_text_font(spinbox, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_align(spinbox, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(spinbox, 5, 0);
    lv_obj_set_style_bg_color(
        spinbox, lv_color_black(), LV_STATE_CHECKED);
    lv_obj_set_style_text_color(
        spinbox, lv_color_white(), LV_STATE_CHECKED);
    lv_obj_add_event_cb(spinbox, callback, LV_EVENT_PRESSED, nullptr);
    return spinbox;
}

static void SelectActiveTime(lv_obj_t *spinbox)
{
    g_editor.active_time = spinbox;
    lv_obj_clear_state(g_editor.hour, LV_STATE_CHECKED);
    lv_obj_clear_state(g_editor.minute, LV_STATE_CHECKED);
    lv_obj_add_state(spinbox, LV_STATE_CHECKED);
    lv_spinbox_set_cursor_pos(spinbox, 1);
}

static void TimeFocusEvent(lv_event_t *event)
{
    SelectActiveTime((lv_obj_t *)lv_event_get_target(event));
}

static void MinusEvent(lv_event_t *event)
{
    (void)event;
    if (g_editor.active_time)
        lv_spinbox_decrement(g_editor.active_time);
}

static void PlusEvent(lv_event_t *event)
{
    (void)event;
    if (g_editor.active_time)
        lv_spinbox_increment(g_editor.active_time);
}

static void StoreCurrentEditorAlarm()
{
    if (g_selected_alarm >= kAlarmCount)
        return;

    AlarmConfig &alarm = g_edit_alarms[g_selected_alarm];
    alarm.enabled =
        lv_obj_has_state(g_editor.enabled, LV_STATE_CHECKED) ? 1 : 0;
    alarm.hour = (uint8_t)lv_spinbox_get_value(g_editor.hour);
    alarm.minute = (uint8_t)lv_spinbox_get_value(g_editor.minute);
    alarm.weekdays = 0;
    for (size_t i = 0; i < 7; ++i)
    {
        if (lv_buttonmatrix_has_button_ctrl(
                g_editor.days_matrix,
                (uint32_t)i,
                LV_BUTTONMATRIX_CTRL_CHECKED))
        {
            alarm.weekdays |= (uint8_t)(1U << i);
        }
    }

    uint32_t sound =
        lv_buttonmatrix_get_selected_button(g_editor.sound_matrix);
    uint32_t volume =
        lv_buttonmatrix_get_selected_button(g_editor.volume_matrix);
    if (sound < kAlarmSoundCount)
        alarm.sound = (uint8_t)sound;
    if (volume < kAlarmVolumeCount)
        alarm.volume = (uint8_t)volume;
}

static void LoadEditorAlarm(size_t alarm_index)
{
    if (alarm_index >= kAlarmCount)
        return;

    const AlarmConfig &alarm = g_edit_alarms[alarm_index];
    lv_spinbox_set_value(g_editor.hour, alarm.hour);
    lv_spinbox_set_value(g_editor.minute, alarm.minute);
    if (alarm.enabled)
        lv_obj_add_state(g_editor.enabled, LV_STATE_CHECKED);
    else
        lv_obj_clear_state(g_editor.enabled, LV_STATE_CHECKED);

    lv_buttonmatrix_clear_button_ctrl_all(
        g_editor.days_matrix, LV_BUTTONMATRIX_CTRL_CHECKED);
    for (size_t i = 0; i < 7; ++i)
    {
        if ((alarm.weekdays & (uint8_t)(1U << i)) != 0)
        {
            lv_buttonmatrix_set_button_ctrl(
                g_editor.days_matrix,
                (uint32_t)i,
                LV_BUTTONMATRIX_CTRL_CHECKED);
        }
    }
    SetMatrixChecked(
        g_editor.sound_matrix, kAlarmSoundCount, alarm.sound);
    SetMatrixChecked(
        g_editor.volume_matrix, kAlarmVolumeCount, alarm.volume);
    SelectActiveTime(g_editor.hour);
}

static void SlotEvent(lv_event_t *event)
{
    (void)event;
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(g_editor.slot_matrix);
    if (selected >= kAlarmCount || selected == g_selected_alarm)
        return;

    StoreCurrentEditorAlarm();
    g_selected_alarm = (size_t)selected;
    LoadEditorAlarm(g_selected_alarm);
}

static void SaveEvent(lv_event_t *event)
{
    (void)event;
    StoreCurrentEditorAlarm();
    memcpy(g_alarms, g_edit_alarms, sizeof(g_alarms));
    if (g_snooze_alarm >= 0 &&
        !g_alarms[(size_t)g_snooze_alarm].enabled)
    {
        g_snooze_alarm = -1;
        g_snooze_at = 0;
    }
    SaveAlarms();
    request_normal_state();
}

static void CancelEvent(lv_event_t *event)
{
    (void)event;
    request_normal_state();
}

static void SnoozeEvent(lv_event_t *event)
{
    (void)event;
    alarm_snooze_current();
}

static void DismissEvent(lv_event_t *event)
{
    (void)event;
    alarm_dismiss_current();
}

static void InitEditorUi(lv_obj_t *screen)
{
    g_editor.panel = lv_obj_create(screen);
    lv_obj_set_size(g_editor.panel, 292, 208);
    lv_obj_center(g_editor.panel);
    lv_obj_set_style_bg_color(g_editor.panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_editor.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_editor.panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_editor.panel, 2, 0);
    lv_obj_set_style_radius(g_editor.panel, 0, 0);
    lv_obj_set_style_pad_all(g_editor.panel, 6, 0);

    lv_obj_t *title = lv_label_create(g_editor.panel);
    lv_label_set_text(title, "Alarms");
    lv_obj_set_style_text_font(title, &lv_font_chicago_8, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    g_editor.slot_matrix = lv_buttonmatrix_create(g_editor.panel);
    lv_buttonmatrix_set_map(g_editor.slot_matrix, g_slot_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_editor.slot_matrix, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(g_editor.slot_matrix, true);
    lv_obj_set_size(g_editor.slot_matrix, 105, 24);
    lv_obj_align(g_editor.slot_matrix, LV_ALIGN_TOP_MID, 0, 14);
    StyleMatrix(g_editor.slot_matrix);
    lv_obj_add_event_cb(
        g_editor.slot_matrix, SlotEvent, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *minus =
        CreateButton(g_editor.panel, "-", MinusEvent);
    lv_obj_set_size(minus, 34, 27);
    lv_obj_align(minus, LV_ALIGN_TOP_LEFT, 0, 44);

    g_editor.hour =
        CreateTimeSpinbox(g_editor.panel, 23, TimeFocusEvent);
    lv_obj_align(g_editor.hour, LV_ALIGN_TOP_LEFT, 40, 44);

    lv_obj_t *colon = lv_label_create(g_editor.panel);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_text_font(colon, &lv_font_chicago_8, 0);
    lv_obj_align(colon, LV_ALIGN_TOP_LEFT, 93, 51);

    g_editor.minute =
        CreateTimeSpinbox(g_editor.panel, 59, TimeFocusEvent);
    lv_obj_align(g_editor.minute, LV_ALIGN_TOP_LEFT, 102, 44);

    lv_obj_t *plus =
        CreateButton(g_editor.panel, "+", PlusEvent);
    lv_obj_set_size(plus, 34, 27);
    lv_obj_align(plus, LV_ALIGN_TOP_LEFT, 156, 44);

    g_editor.enabled = lv_checkbox_create(g_editor.panel);
    lv_checkbox_set_text(g_editor.enabled, "Enabled");
    lv_obj_set_style_text_font(g_editor.enabled, &lv_font_chicago_8, 0);
    lv_obj_align(g_editor.enabled, LV_ALIGN_TOP_RIGHT, 0, 49);

    lv_obj_t *days_label = lv_label_create(g_editor.panel);
    lv_label_set_text(days_label, "Days");
    lv_obj_set_style_text_font(days_label, &lv_font_chicago_8, 0);
    lv_obj_align(days_label, LV_ALIGN_TOP_LEFT, 0, 82);

    g_editor.days_matrix = lv_buttonmatrix_create(g_editor.panel);
    lv_buttonmatrix_set_map(g_editor.days_matrix, g_days_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_editor.days_matrix, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(g_editor.days_matrix, false);
    lv_obj_set_size(g_editor.days_matrix, 240, 25);
    lv_obj_align(g_editor.days_matrix, LV_ALIGN_TOP_RIGHT, 0, 76);
    StyleMatrix(g_editor.days_matrix);

    lv_obj_t *sound_label = lv_label_create(g_editor.panel);
    lv_label_set_text(sound_label, "Sound");
    lv_obj_set_style_text_font(sound_label, &lv_font_chicago_8, 0);
    lv_obj_align(sound_label, LV_ALIGN_TOP_LEFT, 0, 111);

    g_editor.sound_matrix = lv_buttonmatrix_create(g_editor.panel);
    lv_buttonmatrix_set_map(g_editor.sound_matrix, g_sound_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_editor.sound_matrix, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(g_editor.sound_matrix, true);
    lv_obj_set_size(g_editor.sound_matrix, 232, 25);
    lv_obj_align(g_editor.sound_matrix, LV_ALIGN_TOP_RIGHT, 0, 105);
    StyleMatrix(g_editor.sound_matrix);

    lv_obj_t *volume_label = lv_label_create(g_editor.panel);
    lv_label_set_text(volume_label, "Volume");
    lv_obj_set_style_text_font(volume_label, &lv_font_chicago_8, 0);
    lv_obj_align(volume_label, LV_ALIGN_TOP_LEFT, 0, 140);

    g_editor.volume_matrix = lv_buttonmatrix_create(g_editor.panel);
    lv_buttonmatrix_set_map(g_editor.volume_matrix, g_volume_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_editor.volume_matrix, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(g_editor.volume_matrix, true);
    lv_obj_set_size(g_editor.volume_matrix, 224, 25);
    lv_obj_align(g_editor.volume_matrix, LV_ALIGN_TOP_RIGHT, 0, 134);
    StyleMatrix(g_editor.volume_matrix);

    lv_obj_t *save = CreateButton(g_editor.panel, "Save", SaveEvent);
    lv_obj_set_size(save, 110, 27);
    lv_obj_align(save, LV_ALIGN_BOTTOM_LEFT, 12, 0);

    lv_obj_t *cancel =
        CreateButton(g_editor.panel, "Cancel", CancelEvent);
    lv_obj_set_size(cancel, 110, 27);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_RIGHT, -12, 0);

    lv_obj_add_flag(g_editor.panel, LV_OBJ_FLAG_HIDDEN);
}

static void InitRingingUi(lv_obj_t *screen)
{
    g_ringing.panel = lv_obj_create(screen);
    lv_obj_set_size(g_ringing.panel, 286, 180);
    lv_obj_center(g_ringing.panel);
    lv_obj_set_style_bg_color(g_ringing.panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_ringing.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_ringing.panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_ringing.panel, 3, 0);
    lv_obj_set_style_radius(g_ringing.panel, 0, 0);
    lv_obj_set_style_pad_all(g_ringing.panel, 8, 0);

    g_ringing.title = lv_label_create(g_ringing.panel);
    lv_label_set_text(g_ringing.title, "Alarm");
    lv_obj_set_style_text_font(g_ringing.title, &lv_font_chicago_8, 0);
    lv_obj_align(g_ringing.title, LV_ALIGN_TOP_MID, 0, 0);

    g_ringing.time = lv_label_create(g_ringing.panel);
    lv_label_set_text(g_ringing.time, "00:00");
    lv_obj_set_style_text_font(g_ringing.time, &lv_font_chicago_48, 0);
    lv_obj_align(g_ringing.time, LV_ALIGN_TOP_MID, 0, 18);

    g_ringing.sound = lv_label_create(g_ringing.panel);
    lv_label_set_text(g_ringing.sound, "Quack");
    lv_obj_set_style_text_font(g_ringing.sound, &lv_font_chicago_8, 0);
    lv_obj_align(g_ringing.sound, LV_ALIGN_TOP_MID, 0, 73);

    lv_obj_t *snooze =
        CreateButton(g_ringing.panel, "Snooze 9 min", SnoozeEvent);
    lv_obj_set_size(snooze, 122, 32);
    lv_obj_align(snooze, LV_ALIGN_TOP_LEFT, 0, 96);

    lv_obj_t *dismiss =
        CreateButton(g_ringing.panel, "Dismiss", DismissEvent);
    lv_obj_set_size(dismiss, 122, 32);
    lv_obj_align(dismiss, LV_ALIGN_TOP_RIGHT, 0, 96);

    lv_obj_t *help = lv_label_create(g_ringing.panel);
    lv_label_set_text(help, "Alarm: Snooze    Clock: Dismiss");
    lv_obj_set_style_text_font(help, &lv_font_chicago_8, 0);
    lv_obj_align(help, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_add_flag(g_ringing.panel, LV_OBJ_FLAG_HIDDEN);
}
}

void alarms_init(Preferences &preferences)
{
    g_preferences = &preferences;
    SetAlarmDefaults();
    g_snooze_alarm = -1;
    g_snooze_at = 0;

    if (preferences.getBytesLength("alarms_v1") != sizeof(AlarmStorage))
        return;

    AlarmStorage storage = {};
    if (preferences.getBytes(
            "alarms_v1", &storage, sizeof(storage)) != sizeof(storage))
    {
        return;
    }
    if (storage.magic != kAlarmStorageMagic ||
        storage.version != kAlarmStorageVersion)
    {
        return;
    }
    for (size_t i = 0; i < kAlarmCount; ++i)
    {
        if (!AlarmConfigIsValid(storage.alarms[i]))
            return;
    }
    memcpy(g_alarms, storage.alarms, sizeof(g_alarms));
}

int alarms_due(const DateTime &now)
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
    const uint8_t weekday_bit = AlarmWeekdayBit(now);
    int first_due_alarm = -1;
    for (size_t i = 0; i < kAlarmCount; ++i)
    {
        const AlarmConfig &alarm = g_alarms[i];
        if (!alarm.enabled ||
            alarm.hour != now.hour() ||
            alarm.minute != now.minute() ||
            (alarm.weekdays & weekday_bit) == 0 ||
            g_last_trigger_minute[i] == minute_stamp)
        {
            continue;
        }

        g_last_trigger_minute[i] = minute_stamp;
        if (first_due_alarm < 0)
            first_due_alarm = (int)i;
    }
    return first_due_alarm;
}

void alarms_snooze(size_t alarm_index, const DateTime &now)
{
    if (alarm_index >= kAlarmCount)
        return;
    g_snooze_alarm = (int)alarm_index;
    g_snooze_at = now.unixtime() + kAlarmSnoozeSeconds;
}

void alarms_dismiss()
{
    g_snooze_alarm = -1;
    g_snooze_at = 0;
}

bool alarms_have_active_indicator()
{
    if (g_snooze_alarm >= 0)
        return true;
    for (size_t i = 0; i < kAlarmCount; ++i)
    {
        if (g_alarms[i].enabled)
            return true;
    }
    return false;
}

const char *alarms_sound_path(size_t alarm_index)
{
    if (alarm_index >= kAlarmCount)
        return g_sound_paths[0];
    return g_sound_paths[g_alarms[alarm_index].sound];
}

uint8_t alarms_volume(size_t alarm_index)
{
    if (alarm_index >= kAlarmCount)
        return g_volume_values[2];
    return g_volume_values[g_alarms[alarm_index].volume];
}

void alarm_ui_init(lv_obj_t *screen)
{
    InitEditorUi(screen);
    InitRingingUi(screen);
}

void alarm_ui_hide()
{
    if (g_editor.panel)
        lv_obj_add_flag(g_editor.panel, LV_OBJ_FLAG_HIDDEN);
    if (g_ringing.panel)
        lv_obj_add_flag(g_ringing.panel, LV_OBJ_FLAG_HIDDEN);
}

void alarm_ui_enter()
{
    memcpy(g_edit_alarms, g_alarms, sizeof(g_edit_alarms));
    g_selected_alarm = 0;
    SetMatrixChecked(g_editor.slot_matrix, kAlarmCount, g_selected_alarm);
    LoadEditorAlarm(g_selected_alarm);
}

void alarm_ui_show_editor()
{
    if (g_editor.panel)
        lv_obj_clear_flag(g_editor.panel, LV_OBJ_FLAG_HIDDEN);
}

void alarm_ui_show_ringing(size_t alarm_index)
{
    if (!g_ringing.panel || alarm_index >= kAlarmCount)
        return;

    const AlarmConfig &alarm = g_alarms[alarm_index];
    char title[24];
    char time[8];
    snprintf(title, sizeof(title), "Alarm %u", (unsigned)alarm_index + 1);
    snprintf(time, sizeof(time), "%02u:%02u",
             (unsigned)alarm.hour, (unsigned)alarm.minute);
    lv_label_set_text(g_ringing.title, title);
    lv_label_set_text(g_ringing.time, time);
    lv_label_set_text(g_ringing.sound, g_sound_map[alarm.sound]);
    lv_obj_clear_flag(g_ringing.panel, LV_OBJ_FLAG_HIDDEN);
}
