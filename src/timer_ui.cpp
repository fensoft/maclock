#include "timer_ui.h"

#include <Arduino.h>
#include <stdio.h>

LV_FONT_DECLARE(lv_font_chicago_8);
LV_FONT_DECLARE(lv_font_chicago_48);

extern void request_normal_state();
extern void timer_dismiss_current();

namespace
{
static constexpr size_t kPresetCount = 4;

struct TimerUi
{
    lv_obj_t *panel;
    lv_obj_t *presets;
    lv_obj_t *countdown;
    lv_obj_t *status;
    lv_obj_t *minus_button;
    lv_obj_t *plus_button;
    lv_obj_t *stop_button;
    lv_obj_t *finished_panel;
};

static bool g_timer_active = false;
static bool g_timer_finished_pending = false;
static uint32_t g_timer_end_ms = 0;
static uint32_t g_last_ui_seconds = UINT32_MAX;
static uint16_t g_selected_minutes = 25;
static TimerUi g_timer_ui = {};

static const uint16_t g_preset_minutes[kPresetCount] = {5, 15, 25, 60};
static const char *g_preset_map[] = {
    "5 min", "15 min", "\n", "25 min", "60 min", ""};

static int FindPreset(uint16_t minutes)
{
    for (size_t i = 0; i < kPresetCount; ++i)
    {
        if (g_preset_minutes[i] == minutes)
            return (int)i;
    }
    return -1;
}

static void SyncPresetSelection()
{
    if (!g_timer_ui.presets)
        return;

    lv_buttonmatrix_clear_button_ctrl_all(
        g_timer_ui.presets, LV_BUTTONMATRIX_CTRL_CHECKED);
    const int preset = FindPreset(g_selected_minutes);
    if (preset >= 0)
    {
        lv_buttonmatrix_set_button_ctrl(
            g_timer_ui.presets,
            (uint32_t)preset,
            LV_BUTTONMATRIX_CTRL_CHECKED);
        lv_buttonmatrix_set_selected_button(
            g_timer_ui.presets, (uint32_t)preset);
    }
}

static void FormatSeconds(uint32_t total_seconds,
                          char *text,
                          size_t text_size)
{
    const uint32_t minutes = total_seconds / 60;
    const uint32_t seconds = total_seconds % 60;
    snprintf(text, text_size, "%02lu:%02lu",
             (unsigned long)minutes,
             (unsigned long)seconds);
}

static void SetCountdownSeconds(uint32_t total_seconds)
{
    if (total_seconds == g_last_ui_seconds)
        return;

    char countdown[12];
    FormatSeconds(total_seconds, countdown, sizeof(countdown));
    lv_label_set_text(g_timer_ui.countdown, countdown);
    g_last_ui_seconds = total_seconds;
}

static void StartSelectedTimer()
{
    const uint32_t duration_ms =
        (uint32_t)g_selected_minutes * 60U * 1000U;
    g_timer_end_ms = millis() + duration_ms;
    g_timer_active = true;
    g_timer_finished_pending = false;
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

static void StylePresetMatrix(lv_obj_t *matrix)
{
    const lv_style_selector_t checked_items =
        (lv_style_selector_t)LV_PART_ITEMS |
        (lv_style_selector_t)LV_STATE_CHECKED;

    lv_obj_set_style_bg_color(matrix, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(matrix, lv_color_black(), 0);
    lv_obj_set_style_border_width(matrix, 1, 0);
    lv_obj_set_style_radius(matrix, 0, 0);
    lv_obj_set_style_pad_all(matrix, 2, 0);
    lv_obj_set_style_pad_row(matrix, 2, 0);
    lv_obj_set_style_pad_column(matrix, 2, 0);
    lv_obj_set_style_text_font(matrix, &lv_font_chicago_8, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_border_color(matrix, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(matrix, 1, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, lv_color_black(), checked_items);
    lv_obj_set_style_text_color(matrix, lv_color_white(), checked_items);
}

static void PresetEvent(lv_event_t *event)
{
    (void)event;
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(g_timer_ui.presets);
    if (selected < kPresetCount)
    {
        g_selected_minutes = g_preset_minutes[selected];
        if (!timer_is_active())
            SetCountdownSeconds((uint32_t)g_selected_minutes * 60U);
    }
}

static void AdjustMinutes(int delta)
{
    if (timer_is_active())
        return;

    int minutes = (int)g_selected_minutes + delta;
    if (minutes < 1)
        minutes = 1;
    if (minutes > 99)
        minutes = 99;
    g_selected_minutes = (uint16_t)minutes;
    SyncPresetSelection();
    SetCountdownSeconds((uint32_t)g_selected_minutes * 60U);
}

static void MinusEvent(lv_event_t *event)
{
    (void)event;
    AdjustMinutes(-1);
}

static void PlusEvent(lv_event_t *event)
{
    (void)event;
    AdjustMinutes(1);
}

static void StartEvent(lv_event_t *event)
{
    (void)event;
    StartSelectedTimer();
    request_normal_state();
}

static void StopEvent(lv_event_t *event)
{
    (void)event;
    timer_cancel();
    timer_ui_enter(millis());
}

static void BackEvent(lv_event_t *event)
{
    (void)event;
    request_normal_state();
}

static void DismissEvent(lv_event_t *event)
{
    (void)event;
    timer_dismiss_current();
}

static void InitTimerPanel(lv_obj_t *screen)
{
    g_timer_ui.panel = lv_obj_create(screen);
    lv_obj_set_size(g_timer_ui.panel, 286, 196);
    lv_obj_center(g_timer_ui.panel);
    lv_obj_set_style_bg_color(g_timer_ui.panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_timer_ui.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_timer_ui.panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_timer_ui.panel, 3, 0);
    lv_obj_set_style_radius(g_timer_ui.panel, 0, 0);
    lv_obj_set_style_pad_all(g_timer_ui.panel, 8, 0);

    lv_obj_t *title = lv_label_create(g_timer_ui.panel);
    lv_label_set_text(title, "Timer / Pomodoro");
    lv_obj_set_style_text_font(title, &lv_font_chicago_8, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    g_timer_ui.countdown = lv_label_create(g_timer_ui.panel);
    lv_label_set_text(g_timer_ui.countdown, "25:00");
    lv_obj_set_style_text_font(
        g_timer_ui.countdown, &lv_font_chicago_48, 0);
    lv_obj_align(g_timer_ui.countdown, LV_ALIGN_TOP_MID, 0, 17);

    g_timer_ui.minus_button =
        CreateButton(g_timer_ui.panel, "-", MinusEvent);
    lv_obj_set_size(g_timer_ui.minus_button, 38, 32);
    lv_obj_align(g_timer_ui.minus_button, LV_ALIGN_TOP_LEFT, 0, 26);

    g_timer_ui.plus_button =
        CreateButton(g_timer_ui.panel, "+", PlusEvent);
    lv_obj_set_size(g_timer_ui.plus_button, 38, 32);
    lv_obj_align(g_timer_ui.plus_button, LV_ALIGN_TOP_RIGHT, 0, 26);

    g_timer_ui.status = lv_label_create(g_timer_ui.panel);
    lv_label_set_text(g_timer_ui.status, "Select or adjust duration");
    lv_obj_set_style_text_font(
        g_timer_ui.status, &lv_font_chicago_8, 0);
    lv_obj_align(g_timer_ui.status, LV_ALIGN_TOP_MID, 0, 67);

    g_timer_ui.presets = lv_buttonmatrix_create(g_timer_ui.panel);
    lv_buttonmatrix_set_map(g_timer_ui.presets, g_preset_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_timer_ui.presets, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_one_checked(g_timer_ui.presets, true);
    lv_obj_set_size(g_timer_ui.presets, 240, 58);
    lv_obj_align(g_timer_ui.presets, LV_ALIGN_TOP_MID, 0, 81);
    StylePresetMatrix(g_timer_ui.presets);
    lv_obj_add_event_cb(
        g_timer_ui.presets,
        PresetEvent,
        LV_EVENT_VALUE_CHANGED,
        nullptr);

    lv_obj_t *start =
        CreateButton(g_timer_ui.panel, "Start", StartEvent);
    lv_obj_set_size(start, 78, 28);
    lv_obj_align(start, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    g_timer_ui.stop_button =
        CreateButton(g_timer_ui.panel, "Stop", StopEvent);
    lv_obj_set_size(g_timer_ui.stop_button, 78, 28);
    lv_obj_align(g_timer_ui.stop_button, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *back =
        CreateButton(g_timer_ui.panel, "Back", BackEvent);
    lv_obj_set_size(back, 78, 28);
    lv_obj_align(back, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    lv_obj_add_flag(g_timer_ui.panel, LV_OBJ_FLAG_HIDDEN);
}

static void InitFinishedPanel(lv_obj_t *screen)
{
    g_timer_ui.finished_panel = lv_obj_create(screen);
    lv_obj_set_size(g_timer_ui.finished_panel, 286, 180);
    lv_obj_center(g_timer_ui.finished_panel);
    lv_obj_set_style_bg_color(
        g_timer_ui.finished_panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        g_timer_ui.finished_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        g_timer_ui.finished_panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_timer_ui.finished_panel, 3, 0);
    lv_obj_set_style_radius(g_timer_ui.finished_panel, 0, 0);
    lv_obj_set_style_pad_all(g_timer_ui.finished_panel, 8, 0);

    lv_obj_t *title = lv_label_create(g_timer_ui.finished_panel);
    lv_label_set_text(title, "Timer Complete");
    lv_obj_set_style_text_font(title, &lv_font_chicago_8, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *countdown = lv_label_create(g_timer_ui.finished_panel);
    lv_label_set_text(countdown, "00:00");
    lv_obj_set_style_text_font(countdown, &lv_font_chicago_48, 0);
    lv_obj_align(countdown, LV_ALIGN_TOP_MID, 0, 25);

    lv_obj_t *dismiss =
        CreateButton(g_timer_ui.finished_panel, "Dismiss", DismissEvent);
    lv_obj_set_size(dismiss, 150, 34);
    lv_obj_align(dismiss, LV_ALIGN_TOP_MID, 0, 91);

    lv_obj_t *help = lv_label_create(g_timer_ui.finished_panel);
    lv_label_set_text(help, "Press Clock or Alarm to dismiss");
    lv_obj_set_style_text_font(help, &lv_font_chicago_8, 0);
    lv_obj_align(help, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_add_flag(g_timer_ui.finished_panel, LV_OBJ_FLAG_HIDDEN);
}
}

void timer_update(uint32_t now_ms)
{
    if (g_timer_active &&
        (int32_t)(now_ms - g_timer_end_ms) >= 0)
    {
        g_timer_active = false;
        g_timer_finished_pending = true;
    }
}

bool timer_take_finished()
{
    if (!g_timer_finished_pending)
        return false;
    g_timer_finished_pending = false;
    return true;
}

bool timer_is_active()
{
    return g_timer_active;
}

uint32_t timer_remaining_seconds(uint32_t now_ms)
{
    if (!g_timer_active ||
        (int32_t)(now_ms - g_timer_end_ms) >= 0)
    {
        return 0;
    }

    const uint32_t remaining_ms = g_timer_end_ms - now_ms;
    return (remaining_ms + 999U) / 1000U;
}

void timer_format_remaining(uint32_t now_ms,
                            char *text,
                            size_t text_size)
{
    FormatSeconds(timer_remaining_seconds(now_ms), text, text_size);
}

void timer_cancel()
{
    g_timer_active = false;
    g_timer_finished_pending = false;
    g_timer_end_ms = 0;
}

const char *timer_sound_path()
{
    return "/quack.mp3";
}

uint8_t timer_volume()
{
    return 75;
}

void timer_ui_init(lv_obj_t *screen)
{
    InitTimerPanel(screen);
    InitFinishedPanel(screen);
}

void timer_ui_hide()
{
    if (g_timer_ui.panel)
        lv_obj_add_flag(g_timer_ui.panel, LV_OBJ_FLAG_HIDDEN);
    if (g_timer_ui.finished_panel)
        lv_obj_add_flag(g_timer_ui.finished_panel, LV_OBJ_FLAG_HIDDEN);
}

void timer_ui_enter(uint32_t now_ms)
{
    SyncPresetSelection();
    g_last_ui_seconds = UINT32_MAX;
    if (timer_is_active())
    {
        SetCountdownSeconds(timer_remaining_seconds(now_ms));
        lv_label_set_text(g_timer_ui.status, "Running in background");
        lv_obj_clear_flag(
            g_timer_ui.stop_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(
            g_timer_ui.minus_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(
            g_timer_ui.plus_button, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        SetCountdownSeconds(
            (uint32_t)g_selected_minutes * 60U);
        lv_label_set_text(g_timer_ui.status, "Select or adjust duration");
        lv_obj_add_flag(
            g_timer_ui.stop_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(
            g_timer_ui.minus_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(
            g_timer_ui.plus_button, LV_OBJ_FLAG_HIDDEN);
    }
}

void timer_ui_show(uint32_t now_ms)
{
    if (!g_timer_ui.panel)
        return;

    if (timer_is_active())
    {
        SetCountdownSeconds(timer_remaining_seconds(now_ms));
    }
    else
    {
        SetCountdownSeconds(
            (uint32_t)g_selected_minutes * 60U);
    }
    lv_obj_clear_flag(g_timer_ui.panel, LV_OBJ_FLAG_HIDDEN);
}

void timer_ui_show_finished()
{
    if (g_timer_ui.finished_panel)
    {
        lv_obj_clear_flag(
            g_timer_ui.finished_panel, LV_OBJ_FLAG_HIDDEN);
    }
}
