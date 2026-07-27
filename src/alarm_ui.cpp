#include "alarm_ui.h"
#include "localization.h"
#include "sound_selector.h"

#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(lv_font_chicago_8);
LV_FONT_DECLARE(lv_font_chicago_48);

namespace
{
static constexpr uint32_t kAlarmStorageMagic = 0x414C524D; // 'ALRM'
static constexpr uint8_t kAlarmStorageVersion = 1;
static constexpr uint8_t kAllWeekdays = 0x7F;
static constexpr size_t kLegacyAlarmSoundCount = 3;
static constexpr size_t kAlarmVolumeCount = 4;

enum AlarmEditorPage
{
    ALARM_PAGE_HOME,
    ALARM_PAGE_SELECT,
    ALARM_PAGE_TIME,
    ALARM_PAGE_DAYS,
    ALARM_PAGE_SOUND,
    ALARM_PAGE_VOLUME,
    ALARM_PAGE_ACTIONS,
    ALARM_PAGE_COUNT
};

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
    lv_obj_t *title;
    lv_obj_t *pages[ALARM_PAGE_COUNT];
    lv_obj_t *slot_matrix;
    lv_obj_t *enabled_matrix;
    lv_obj_t *time_value;
    lv_obj_t *time_matrix;
    lv_obj_t *days_matrix;
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

} // namespace

struct AlarmService::State
{
    Preferences *preferences = nullptr;
    AlarmConfig alarms[kAlarmCount] = {};
    AlarmConfig edit_alarms[kAlarmCount] = {};
    char alarm_sound_paths[kAlarmCount]
                          [SOUND_SELECTOR_PATH_MAX] = {};
    char edit_alarm_sound_paths[kAlarmCount]
                               [SOUND_SELECTOR_PATH_MAX] = {};
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
    const char *page_names[ALARM_PAGE_COUNT] = {};
};

namespace
{
AlarmService *active_alarm_service = nullptr;

#define g_preferences (active_alarm_service->state().preferences)
#define g_alarms (active_alarm_service->state().alarms)
#define g_edit_alarms (active_alarm_service->state().edit_alarms)
#define g_alarm_sound_paths \
    (active_alarm_service->state().alarm_sound_paths)
#define g_edit_alarm_sound_paths \
    (active_alarm_service->state().edit_alarm_sound_paths)
#define g_last_trigger_minute \
    (active_alarm_service->state().last_trigger_minute)
#define g_snooze_alarm (active_alarm_service->state().snooze_alarm)
#define g_snooze_at (active_alarm_service->state().snooze_at)
#define g_selected_alarm \
    (active_alarm_service->state().selected_alarm)
#define g_editor_page (active_alarm_service->state().editor_page)
#define g_editor (active_alarm_service->state().editor)
#define g_ringing (active_alarm_service->state().ringing)
#define g_events (active_alarm_service->state().events)
#define g_slot_map (active_alarm_service->state().slot_map)
#define g_enabled_map (active_alarm_service->state().enabled_map)
#define g_time_map (active_alarm_service->state().time_map)
#define g_days_map (active_alarm_service->state().days_map)
#define g_page_names (active_alarm_service->state().page_names)

static const char *g_volume_map[] = {
    "25%", "50%", "\n", "75%", "100%", ""};
static const char *g_legacy_sound_paths[kLegacyAlarmSoundCount] = {
    "/quack.mp3",
    "/startup.mp3",
    "/floppy.mp3"};
static const uint8_t g_volume_values[kAlarmVolumeCount] = {25, 50, 75, 100};

static void UpdateLanguageMaps()
{
    g_slot_map[0] = tr("Alarm 1");
    g_slot_map[1] = tr("Alarm 2");
    g_slot_map[2] = tr("Alarm 3");
    g_slot_map[3] = "";
    g_enabled_map[0] = tr("Disabled");
    g_enabled_map[1] = tr("Enabled");
    g_enabled_map[2] = "";
    g_time_map[0] = tr("Hour -");
    g_time_map[1] = tr("Hour +");
    g_time_map[2] = tr("Minute -");
    g_time_map[3] = tr("Minute +");
    g_time_map[4] = "";
    g_days_map[0] = tr("Mon");
    g_days_map[1] = tr("Tue");
    g_days_map[2] = tr("Wed");
    g_days_map[3] = tr("Thu");
    g_days_map[4] = "\n";
    g_days_map[5] = tr("Fri");
    g_days_map[6] = tr("Sat");
    g_days_map[7] = tr("Sun");
    g_days_map[8] = "";
    g_page_names[0] = tr("Alarm / Timer");
    g_page_names[1] = tr("Alarm");
    g_page_names[2] = tr("Time");
    g_page_names[3] = tr("Days");
    g_page_names[4] = tr("Sound");
    g_page_names[5] = tr("Volume");
    g_page_names[6] = tr("Actions");
}

static void SetAlarmDefaults()
{
    memset(g_alarms, 0, sizeof(g_alarms));
    for (size_t i = 0; i < kAlarmCount; ++i)
    {
        g_alarms[i].hour = (uint8_t)(7 + i);
        g_alarms[i].weekdays = kAllWeekdays;
        g_alarms[i].sound = 0;
        g_alarms[i].volume = 2;
        strlcpy(
            g_alarm_sound_paths[i], "/quack.mp3",
            SOUND_SELECTOR_PATH_MAX);
        g_last_trigger_minute[i] = UINT32_MAX;
    }
}

static bool AlarmConfigIsValid(const AlarmConfig &alarm)
{
    return alarm.enabled <= 1 &&
           alarm.hour < 24 &&
           alarm.minute < 60 &&
           (alarm.weekdays & ~kAllWeekdays) == 0 &&
           alarm.sound < kLegacyAlarmSoundCount &&
           alarm.volume < kAlarmVolumeCount;
}

static void LoadAlarmSoundPaths(Preferences &preferences)
{
    for (size_t i = 0; i < kAlarmCount; ++i)
    {
        char key[20];
        snprintf(
            key, sizeof(key), "alarm_sound_%u",
            (unsigned)i);
        const uint8_t legacy_sound =
            g_alarms[i].sound < kLegacyAlarmSoundCount
                ? g_alarms[i].sound
                : 0;
        const String saved = preferences.getString(
            key, g_legacy_sound_paths[legacy_sound]);
        strlcpy(
            g_alarm_sound_paths[i], saved.c_str(),
            SOUND_SELECTOR_PATH_MAX);
    }
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
    for (size_t i = 0; i < kAlarmCount; ++i)
    {
        char key[20];
        snprintf(
            key, sizeof(key), "alarm_sound_%u",
            (unsigned)i);
        g_preferences->putString(
            key, g_alarm_sound_paths[i]);
    }
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
    const lv_style_selector_t pressed_items =
        (lv_style_selector_t)LV_PART_ITEMS |
        (lv_style_selector_t)LV_STATE_PRESSED;

    lv_obj_set_style_bg_opa(matrix, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(matrix, 0, 0);
    lv_obj_set_style_radius(matrix, 0, 0);
    lv_obj_set_style_pad_all(matrix, 0, 0);
    lv_obj_set_style_pad_row(matrix, 6, 0);
    lv_obj_set_style_pad_column(matrix, 6, 0);
    lv_obj_set_style_text_font(matrix, &lv_font_chicago_8, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_border_color(matrix, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(matrix, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(matrix, 4, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(matrix, 0, LV_PART_ITEMS);
    lv_obj_set_style_outline_width(matrix, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, lv_color_black(), checked_items);
    lv_obj_set_style_text_color(matrix, lv_color_white(), checked_items);
    lv_obj_set_style_bg_color(matrix, lv_color_black(), pressed_items);
    lv_obj_set_style_text_color(matrix, lv_color_white(), pressed_items);
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
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_radius(button, 4, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_outline_width(button, 0, 0);
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

static void UpdateTimeValue()
{
    if (!g_editor.time_value || g_selected_alarm >= kAlarmCount)
        return;

    const AlarmConfig &alarm = g_edit_alarms[g_selected_alarm];
    char text[8];
    snprintf(text, sizeof(text), "%02u:%02u",
             (unsigned)alarm.hour, (unsigned)alarm.minute);
    lv_label_set_text(g_editor.time_value, text);
}

static void UpdateSummary()
{
    if (!g_editor.summary || g_selected_alarm >= kAlarmCount)
        return;

    const AlarmConfig &alarm = g_edit_alarms[g_selected_alarm];
    static const char *day_names[] = {
        "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    char days[48] = {};
    size_t day_count = 0;
    for (size_t i = 0; i < 7; ++i)
    {
        if ((alarm.weekdays & (uint8_t)(1U << i)) != 0)
        {
            if (day_count++)
                strlcat(days, " ", sizeof(days));
            strlcat(days, tr(day_names[i]), sizeof(days));
        }
    }
    if (day_count == 0)
        strlcpy(days, "-", sizeof(days));

    char text[160];
    snprintf(text, sizeof(text),
             "%s %u: %02u:%02u  %s\n%s: %s\n%s  %u%%",
             tr("Alarm"),
             (unsigned)g_selected_alarm + 1,
             (unsigned)alarm.hour,
             (unsigned)alarm.minute,
             alarm.enabled ? tr("Enabled") : tr("Disabled"),
             tr("Days"),
             days,
             SoundSelector::displayName(
                 g_edit_alarm_sound_paths[g_selected_alarm]),
             (unsigned)g_volume_values[alarm.volume]);
    lv_label_set_text(g_editor.summary, text);
}

static void TimeEvent(lv_event_t *event)
{
    (void)event;
    if (g_selected_alarm >= kAlarmCount)
        return;

    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(g_editor.time_matrix);
    AlarmConfig &alarm = g_edit_alarms[g_selected_alarm];
    switch (selected)
    {
    case 0:
        alarm.hour = (uint8_t)((alarm.hour + 23) % 24);
        break;
    case 1:
        alarm.hour = (uint8_t)((alarm.hour + 1) % 24);
        break;
    case 2:
        alarm.minute = (uint8_t)((alarm.minute + 59) % 60);
        break;
    case 3:
        alarm.minute = (uint8_t)((alarm.minute + 1) % 60);
        break;
    default:
        return;
    }
    UpdateTimeValue();
}

static void EnabledEvent(lv_event_t *event)
{
    (void)event;
    if (g_selected_alarm >= kAlarmCount)
        return;

    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(g_editor.enabled_matrix);
    if (selected < 2)
        g_edit_alarms[g_selected_alarm].enabled = selected == 1;
}

static void DaysEvent(lv_event_t *event)
{
    (void)event;
    if (g_selected_alarm >= kAlarmCount)
        return;

    AlarmConfig &alarm = g_edit_alarms[g_selected_alarm];
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
}

static void SoundChanged(const char *path, void *user_data)
{
    (void)user_data;
    if (!path || g_selected_alarm >= kAlarmCount)
        return;
    strlcpy(
        g_edit_alarm_sound_paths[g_selected_alarm], path,
        SOUND_SELECTOR_PATH_MAX);
}

static void VolumeEvent(lv_event_t *event)
{
    (void)event;
    if (g_selected_alarm >= kAlarmCount)
        return;

    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(g_editor.volume_matrix);
    if (selected < kAlarmVolumeCount)
    {
        g_edit_alarms[g_selected_alarm].volume = (uint8_t)selected;
        g_editor.sound_selector.setPreviewVolume(
            g_volume_values[selected]);
    }
}

static void LoadEditorAlarm(size_t alarm_index)
{
    if (alarm_index >= kAlarmCount)
        return;

    const AlarmConfig &alarm = g_edit_alarms[alarm_index];
    SetMatrixChecked(g_editor.enabled_matrix, 2, alarm.enabled ? 1 : 0);

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
    g_editor.sound_selector.setPath(
        g_edit_alarm_sound_paths[alarm_index]);
    const char *resolved_sound =
        g_editor.sound_selector.path();
    if (resolved_sound)
    {
        strlcpy(
            g_edit_alarm_sound_paths[alarm_index],
            resolved_sound, SOUND_SELECTOR_PATH_MAX);
    }
    g_editor.sound_selector.setPreviewVolume(
        g_volume_values[alarm.volume]);
    SetMatrixChecked(
        g_editor.volume_matrix, kAlarmVolumeCount, alarm.volume);
    UpdateTimeValue();
    UpdateSummary();
}

static void SlotEvent(lv_event_t *event)
{
    (void)event;
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(g_editor.slot_matrix);
    if (selected >= kAlarmCount || selected == g_selected_alarm)
        return;

    g_selected_alarm = (size_t)selected;
    LoadEditorAlarm(g_selected_alarm);
}

static void SaveEvent(lv_event_t *event)
{
    (void)event;
    memcpy(g_alarms, g_edit_alarms, sizeof(g_alarms));
    memcpy(
        g_alarm_sound_paths, g_edit_alarm_sound_paths,
        sizeof(g_alarm_sound_paths));
    if (g_snooze_alarm >= 0 &&
        !g_alarms[(size_t)g_snooze_alarm].enabled)
    {
        g_snooze_alarm = -1;
        g_snooze_at = 0;
    }
    SaveAlarms();
    if (g_events)
        g_events->requestState(UiState::Normal);
}

static void CancelEvent(lv_event_t *event)
{
    (void)event;
    if (g_events)
        g_events->requestState(UiState::Normal);
}

static void SetEditorPage(AlarmEditorPage page);

static void OpenTimerEvent(lv_event_t *event)
{
    (void)event;
    if (g_events)
        g_events->requestState(UiState::TimerEditor);
}

static void OpenAlarmSettingsEvent(lv_event_t *event)
{
    (void)event;
    SetEditorPage(ALARM_PAGE_SELECT);
}

static void SetEditorPage(AlarmEditorPage page)
{
    if (page >= ALARM_PAGE_COUNT)
        return;

    g_editor_page = page;
    for (size_t i = 0; i < ALARM_PAGE_COUNT; ++i)
        lv_obj_add_flag(g_editor.pages[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_editor.pages[page], LV_OBJ_FLAG_HIDDEN);

    char title[40];
    snprintf(title, sizeof(title), "%s - %s (%u/%u)",
             tr("Alarms"), g_page_names[page],
             (unsigned)page + 1,
             (unsigned)ALARM_PAGE_COUNT);
    lv_label_set_text(g_editor.title, title);

    if (page == ALARM_PAGE_HOME)
        lv_obj_add_flag(g_editor.previous, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(g_editor.previous, LV_OBJ_FLAG_HIDDEN);

    if (page == ALARM_PAGE_ACTIONS)
        lv_obj_add_flag(g_editor.next, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(g_editor.next, LV_OBJ_FLAG_HIDDEN);

    if (page == ALARM_PAGE_ACTIONS)
        UpdateSummary();
}

static void PreviousPageEvent(lv_event_t *event)
{
    (void)event;
    if (g_editor_page > ALARM_PAGE_HOME)
        SetEditorPage((AlarmEditorPage)(g_editor_page - 1));
}

static void NextPageEvent(lv_event_t *event)
{
    (void)event;
    if (g_editor_page < ALARM_PAGE_ACTIONS)
        SetEditorPage((AlarmEditorPage)(g_editor_page + 1));
}

static void SnoozeEvent(lv_event_t *event)
{
    (void)event;
    if (g_events)
        g_events->snoozeActiveAlarm();
}

static void DismissEvent(lv_event_t *event)
{
    (void)event;
    if (g_events)
        g_events->dismissActiveAlarm();
}

static lv_obj_t *CreateEditorPage(lv_obj_t *parent)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, 276, 130);
    lv_obj_align(page, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    return page;
}

static void SetClickOnRelease(lv_obj_t *matrix)
{
    lv_buttonmatrix_set_button_ctrl_all(
        matrix, LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
}

static void InitEditorUi(lv_obj_t *screen)
{
    UpdateLanguageMaps();
    g_editor.panel = lv_obj_create(screen);
    lv_obj_set_size(g_editor.panel, 292, 208);
    lv_obj_center(g_editor.panel);
    lv_obj_set_style_bg_color(g_editor.panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_editor.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_editor.panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_editor.panel, 2, 0);
    lv_obj_set_style_radius(g_editor.panel, 0, 0);
    lv_obj_set_style_pad_all(g_editor.panel, 6, 0);

    g_editor.title = lv_label_create(g_editor.panel);
    lv_label_set_text(g_editor.title, tr("Alarms"));
    lv_obj_set_style_text_font(g_editor.title, &lv_font_chicago_8, 0);
    lv_obj_align(g_editor.title, LV_ALIGN_TOP_MID, 0, 0);

    for (size_t i = 0; i < ALARM_PAGE_COUNT; ++i)
        g_editor.pages[i] = CreateEditorPage(g_editor.panel);

    lv_obj_t *home_page = g_editor.pages[ALARM_PAGE_HOME];
    lv_obj_t *alarm_button =
        CreateButton(home_page, tr("Alarm"),
                     OpenAlarmSettingsEvent);
    g_editor.home_alarm_label = lv_obj_get_child(alarm_button, 0);
    lv_obj_set_size(alarm_button, 124, 124);
    lv_obj_align(alarm_button, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *timer_button =
        CreateButton(home_page, tr("Timer"),
                     OpenTimerEvent);
    g_editor.home_timer_label = lv_obj_get_child(timer_button, 0);
    lv_obj_set_size(timer_button, 124, 124);
    lv_obj_align(timer_button, LV_ALIGN_RIGHT_MID, -8, 0);

    lv_obj_t *select_page = g_editor.pages[ALARM_PAGE_SELECT];
    g_editor.slot_matrix = lv_buttonmatrix_create(select_page);
    lv_buttonmatrix_set_map(g_editor.slot_matrix, g_slot_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_editor.slot_matrix, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(g_editor.slot_matrix, true);
    SetClickOnRelease(g_editor.slot_matrix);
    lv_obj_set_size(g_editor.slot_matrix, 260, 58);
    lv_obj_align(g_editor.slot_matrix, LV_ALIGN_TOP_MID, 0, 0);
    StyleMatrix(g_editor.slot_matrix);
    lv_obj_add_event_cb(
        g_editor.slot_matrix, SlotEvent, LV_EVENT_VALUE_CHANGED, nullptr);

    g_editor.enabled_matrix = lv_buttonmatrix_create(select_page);
    lv_buttonmatrix_set_map(g_editor.enabled_matrix, g_enabled_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_editor.enabled_matrix, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(g_editor.enabled_matrix, true);
    SetClickOnRelease(g_editor.enabled_matrix);
    lv_obj_set_size(g_editor.enabled_matrix, 260, 58);
    lv_obj_align(g_editor.enabled_matrix, LV_ALIGN_BOTTOM_MID, 0, 0);
    StyleMatrix(g_editor.enabled_matrix);
    lv_obj_add_event_cb(
        g_editor.enabled_matrix, EnabledEvent,
        LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *time_page = g_editor.pages[ALARM_PAGE_TIME];
    g_editor.time_value = lv_label_create(time_page);
    lv_label_set_text(g_editor.time_value, "00:00");
    lv_obj_set_style_text_font(
        g_editor.time_value, &lv_font_chicago_48, 0);
    lv_obj_align(g_editor.time_value, LV_ALIGN_TOP_MID, 0, -5);

    g_editor.time_matrix = lv_buttonmatrix_create(time_page);
    lv_buttonmatrix_set_map(g_editor.time_matrix, g_time_map);
    SetClickOnRelease(g_editor.time_matrix);
    lv_obj_set_size(g_editor.time_matrix, 260, 60);
    lv_obj_align(g_editor.time_matrix, LV_ALIGN_BOTTOM_MID, 0, 0);
    StyleMatrix(g_editor.time_matrix);
    lv_obj_add_event_cb(
        g_editor.time_matrix, TimeEvent,
        LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *days_page = g_editor.pages[ALARM_PAGE_DAYS];
    g_editor.days_matrix = lv_buttonmatrix_create(days_page);
    lv_buttonmatrix_set_map(g_editor.days_matrix, g_days_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_editor.days_matrix, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(g_editor.days_matrix, false);
    SetClickOnRelease(g_editor.days_matrix);
    lv_obj_set_size(g_editor.days_matrix, 260, 124);
    lv_obj_center(g_editor.days_matrix);
    StyleMatrix(g_editor.days_matrix);
    lv_obj_add_event_cb(
        g_editor.days_matrix, DaysEvent,
        LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *sound_page = g_editor.pages[ALARM_PAGE_SOUND];
    g_editor.sound_selector.begin(
        sound_page,
        "/quack.mp3",
        g_volume_values[2],
        SoundChanged,
        nullptr);

    lv_obj_t *volume_page = g_editor.pages[ALARM_PAGE_VOLUME];
    g_editor.volume_matrix = lv_buttonmatrix_create(volume_page);
    lv_buttonmatrix_set_map(g_editor.volume_matrix, g_volume_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_editor.volume_matrix, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(g_editor.volume_matrix, true);
    SetClickOnRelease(g_editor.volume_matrix);
    lv_obj_set_size(g_editor.volume_matrix, 260, 124);
    lv_obj_center(g_editor.volume_matrix);
    StyleMatrix(g_editor.volume_matrix);
    lv_obj_add_event_cb(
        g_editor.volume_matrix, VolumeEvent,
        LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *actions_page = g_editor.pages[ALARM_PAGE_ACTIONS];
    g_editor.summary = lv_label_create(actions_page);
    lv_label_set_text(g_editor.summary, "");
    lv_obj_set_width(g_editor.summary, 260);
    lv_obj_set_style_text_font(
        g_editor.summary, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_align(
        g_editor.summary, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(g_editor.summary, 3, 0);
    lv_obj_align(g_editor.summary, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *save =
        CreateButton(actions_page, tr("Save"), SaveEvent);
    g_editor.save_label = lv_obj_get_child(save, 0);
    lv_obj_set_size(save, 260, 58);
    lv_obj_align(save, LV_ALIGN_BOTTOM_MID, 0, 0);

    g_editor.previous =
        CreateButton(g_editor.panel, tr("Previous"), PreviousPageEvent);
    lv_obj_set_size(g_editor.previous, 84, 40);
    lv_obj_align(g_editor.previous, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    g_editor.previous_label =
        lv_obj_get_child(g_editor.previous, 0);

    g_editor.exit =
        CreateButton(g_editor.panel, tr("Exit"), CancelEvent);
    lv_obj_set_size(g_editor.exit, 84, 40);
    lv_obj_align(g_editor.exit, LV_ALIGN_BOTTOM_MID, 0, 0);
    g_editor.exit_label =
        lv_obj_get_child(g_editor.exit, 0);

    g_editor.next =
        CreateButton(g_editor.panel, tr("Next"), NextPageEvent);
    lv_obj_set_size(g_editor.next, 84, 40);
    lv_obj_align(g_editor.next, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    g_editor.next_label = lv_obj_get_child(g_editor.next, 0);

    lv_obj_add_flag(g_editor.panel, LV_OBJ_FLAG_HIDDEN);
    SetEditorPage(ALARM_PAGE_HOME);
}

static void InitRingingUi(lv_obj_t *screen)
{
    g_ringing.panel = lv_obj_create(screen);
    lv_obj_set_size(g_ringing.panel, 286, 200);
    lv_obj_center(g_ringing.panel);
    lv_obj_set_style_bg_color(g_ringing.panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_ringing.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_ringing.panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_ringing.panel, 3, 0);
    lv_obj_set_style_radius(g_ringing.panel, 0, 0);
    lv_obj_set_style_pad_all(g_ringing.panel, 8, 0);

    g_ringing.title = lv_label_create(g_ringing.panel);
    lv_label_set_text(g_ringing.title, tr("Alarm"));
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
        CreateButton(g_ringing.panel, tr("Snooze 9 min"), SnoozeEvent);
    g_ringing.snooze_label = lv_obj_get_child(snooze, 0);
    lv_obj_set_size(snooze, 260, 42);
    lv_obj_align(snooze, LV_ALIGN_TOP_MID, 0, 91);

    lv_obj_t *dismiss =
        CreateButton(g_ringing.panel, tr("Dismiss"), DismissEvent);
    g_ringing.dismiss_label = lv_obj_get_child(dismiss, 0);
    lv_obj_set_size(dismiss, 260, 42);
    lv_obj_align(dismiss, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_add_flag(g_ringing.panel, LV_OBJ_FLAG_HIDDEN);
}
}

AlarmService::State &AlarmService::state()
{
    return *state_;
}

void AlarmService::begin(Preferences &preferences)
{
    if (!state_)
        state_ = new State();
    active_alarm_service = this;
    g_preferences = &preferences;
    SetAlarmDefaults();
    g_snooze_alarm = -1;
    g_snooze_at = 0;

    bool storage_valid =
        preferences.getBytesLength("alarms_v1") ==
        sizeof(AlarmStorage);
    AlarmStorage storage = {};
    if (storage_valid)
    {
        storage_valid =
            preferences.getBytes(
                "alarms_v1", &storage, sizeof(storage)) ==
                sizeof(storage) &&
            storage.magic == kAlarmStorageMagic &&
            storage.version == kAlarmStorageVersion;
    }
    for (size_t i = 0; storage_valid && i < kAlarmCount; ++i)
        storage_valid = AlarmConfigIsValid(storage.alarms[i]);
    if (storage_valid)
        memcpy(g_alarms, storage.alarms, sizeof(g_alarms));

    LoadAlarmSoundPaths(preferences);
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

void AlarmService::snooze(
    size_t alarm_index, const DateTime &now)
{
    if (alarm_index >= kAlarmCount)
        return;
    g_snooze_alarm = (int)alarm_index;
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
    {
        if (g_alarms[i].enabled)
            return true;
    }
    return false;
}

const char *AlarmService::soundPath(size_t alarm_index) const
{
    if (alarm_index >= kAlarmCount)
        return SoundSelector::resolvePath(
            "/quack.mp3", "/quack.mp3");
    return SoundSelector::resolvePath(
        g_alarm_sound_paths[alarm_index], "/quack.mp3");
}

uint8_t AlarmService::volume(size_t alarm_index) const
{
    if (alarm_index >= kAlarmCount)
        return g_volume_values[2];
    return g_volume_values[g_alarms[alarm_index].volume];
}

void AlarmView::begin(lv_obj_t *screen, AppEventSink &events)
{
    g_events = &events;
    InitEditorUi(screen);
    InitRingingUi(screen);
}

void AlarmView::hide()
{
    if (g_editor.panel)
        lv_obj_add_flag(g_editor.panel, LV_OBJ_FLAG_HIDDEN);
    if (g_ringing.panel)
        lv_obj_add_flag(g_ringing.panel, LV_OBJ_FLAG_HIDDEN);
}

void AlarmView::enter()
{
    memcpy(g_edit_alarms, g_alarms, sizeof(g_edit_alarms));
    memcpy(
        g_edit_alarm_sound_paths, g_alarm_sound_paths,
        sizeof(g_edit_alarm_sound_paths));
    g_selected_alarm = 0;
    SetMatrixChecked(g_editor.slot_matrix, kAlarmCount, g_selected_alarm);
    LoadEditorAlarm(g_selected_alarm);
    SetEditorPage(ALARM_PAGE_HOME);
}

void AlarmView::showEditor()
{
    if (g_editor.panel)
        lv_obj_clear_flag(g_editor.panel, LV_OBJ_FLAG_HIDDEN);
}

void AlarmView::showRinging(size_t alarm_index)
{
    if (!g_ringing.panel || alarm_index >= kAlarmCount)
        return;

    const AlarmConfig &alarm = g_alarms[alarm_index];
    char title[24];
    char time[8];
    snprintf(title, sizeof(title), "%s %u", tr("Alarm"),
             (unsigned)alarm_index + 1);
    snprintf(time, sizeof(time), "%02u:%02u",
             (unsigned)alarm.hour, (unsigned)alarm.minute);
    lv_label_set_text(g_ringing.title, title);
    lv_label_set_text(g_ringing.time, time);
    lv_label_set_text(
        g_ringing.sound,
        SoundSelector::displayName(
            g_alarm_sound_paths[alarm_index]));
    lv_obj_clear_flag(g_ringing.panel, LV_OBJ_FLAG_HIDDEN);
}

void AlarmView::refreshLanguage()
{
    UpdateLanguageMaps();
    if (!g_editor.panel)
        return;

    lv_buttonmatrix_set_map(g_editor.slot_matrix, g_slot_map);
    lv_buttonmatrix_set_map(g_editor.enabled_matrix, g_enabled_map);
    lv_buttonmatrix_set_map(g_editor.time_matrix, g_time_map);
    lv_buttonmatrix_set_map(g_editor.days_matrix, g_days_map);
    lv_label_set_text(g_editor.home_alarm_label, tr("Alarm"));
    lv_label_set_text(g_editor.home_timer_label, tr("Timer"));
    lv_label_set_text(g_editor.save_label, tr("Save"));
    lv_label_set_text(g_editor.previous_label, tr("Previous"));
    lv_label_set_text(g_editor.exit_label, tr("Exit"));
    lv_label_set_text(g_editor.next_label, tr("Next"));
    lv_label_set_text(g_ringing.snooze_label, tr("Snooze 9 min"));
    lv_label_set_text(g_ringing.dismiss_label, tr("Dismiss"));
    g_editor.sound_selector.refreshLanguage();
    SetEditorPage(g_editor_page);
    UpdateSummary();
}
