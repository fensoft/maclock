#include "timer_ui.h"
#include "localization.h"

#include <Arduino.h>
#include <stdio.h>

LV_FONT_DECLARE(lv_font_chicago_8);
LV_FONT_DECLARE(lv_font_chicago_48);

extern void request_normal_state();
extern void timer_dismiss_current();

namespace
{
struct TimerUi
{
    lv_obj_t *panel;
    lv_obj_t *adjustments;
    lv_obj_t *countdown;
    lv_obj_t *status;
    lv_obj_t *stop_button;
    lv_obj_t *finished_panel;
    lv_obj_t *title;
    lv_obj_t *start_label;
    lv_obj_t *stop_label;
    lv_obj_t *back_label;
    lv_obj_t *finished_title;
    lv_obj_t *dismiss_label;
    lv_obj_t *finished_help;
};

static bool g_timer_active = false;
static bool g_timer_finished_pending = false;
static uint32_t g_timer_end_ms = 0;
static uint32_t g_last_ui_seconds = UINT32_MAX;
static uint16_t g_selected_minutes = 25;
static TimerUi g_timer_ui = {};

static const char *g_adjustment_map[] = {
    "-10", "-1", "+1", "+10", ""};

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

static void StyleAdjustmentMatrix(lv_obj_t *matrix)
{
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
    lv_obj_set_style_bg_color(matrix, lv_color_black(), pressed_items);
    lv_obj_set_style_text_color(matrix, lv_color_white(), pressed_items);
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
    SetCountdownSeconds((uint32_t)g_selected_minutes * 60U);
}

static void AdjustmentEvent(lv_event_t *event)
{
    (void)event;
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(g_timer_ui.adjustments);
    switch (selected)
    {
    case 0:
        AdjustMinutes(-10);
        break;
    case 1:
        AdjustMinutes(-1);
        break;
    case 2:
        AdjustMinutes(1);
        break;
    case 3:
        AdjustMinutes(10);
        break;
    default:
        break;
    }
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
    lv_obj_set_size(g_timer_ui.panel, 292, 208);
    lv_obj_center(g_timer_ui.panel);
    lv_obj_set_style_bg_color(g_timer_ui.panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_timer_ui.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_timer_ui.panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_timer_ui.panel, 3, 0);
    lv_obj_set_style_radius(g_timer_ui.panel, 0, 0);
    lv_obj_set_style_pad_all(g_timer_ui.panel, 8, 0);

    g_timer_ui.title = lv_label_create(g_timer_ui.panel);
    lv_label_set_text(g_timer_ui.title, tr("Timer"));
    lv_obj_set_style_text_font(g_timer_ui.title, &lv_font_chicago_8, 0);
    lv_obj_align(g_timer_ui.title, LV_ALIGN_TOP_MID, 0, 0);

    g_timer_ui.countdown = lv_label_create(g_timer_ui.panel);
    lv_label_set_text(g_timer_ui.countdown, "25:00");
    lv_obj_set_style_text_font(
        g_timer_ui.countdown, &lv_font_chicago_48, 0);
    lv_obj_align(g_timer_ui.countdown, LV_ALIGN_TOP_MID, 0, 13);

    g_timer_ui.status = lv_label_create(g_timer_ui.panel);
    lv_label_set_text(g_timer_ui.status, tr("Adjust duration"));
    lv_obj_set_style_text_font(
        g_timer_ui.status, &lv_font_chicago_8, 0);
    lv_obj_align(g_timer_ui.status, LV_ALIGN_TOP_MID, 0, 64);

    g_timer_ui.adjustments = lv_buttonmatrix_create(g_timer_ui.panel);
    lv_buttonmatrix_set_map(
        g_timer_ui.adjustments, g_adjustment_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_timer_ui.adjustments, LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_obj_set_size(g_timer_ui.adjustments, 260, 60);
    lv_obj_align(
        g_timer_ui.adjustments, LV_ALIGN_TOP_MID, 0, 80);
    StyleAdjustmentMatrix(g_timer_ui.adjustments);
    lv_obj_add_event_cb(
        g_timer_ui.adjustments,
        AdjustmentEvent,
        LV_EVENT_VALUE_CHANGED,
        nullptr);

    lv_obj_t *start =
        CreateButton(g_timer_ui.panel, tr("Start"), StartEvent);
    g_timer_ui.start_label = lv_obj_get_child(start, 0);
    lv_obj_set_size(start, 84, 40);
    lv_obj_align(start, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    g_timer_ui.stop_button =
        CreateButton(g_timer_ui.panel, tr("Stop"), StopEvent);
    g_timer_ui.stop_label = lv_obj_get_child(g_timer_ui.stop_button, 0);
    lv_obj_set_size(g_timer_ui.stop_button, 84, 40);
    lv_obj_align(g_timer_ui.stop_button, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *back =
        CreateButton(g_timer_ui.panel, tr("Back"), BackEvent);
    g_timer_ui.back_label = lv_obj_get_child(back, 0);
    lv_obj_set_size(back, 84, 40);
    lv_obj_align(back, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    lv_obj_add_flag(g_timer_ui.panel, LV_OBJ_FLAG_HIDDEN);
}

static void InitFinishedPanel(lv_obj_t *screen)
{
    g_timer_ui.finished_panel = lv_obj_create(screen);
    lv_obj_set_size(g_timer_ui.finished_panel, 286, 200);
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

    g_timer_ui.finished_title = lv_label_create(g_timer_ui.finished_panel);
    lv_label_set_text(g_timer_ui.finished_title, tr("Timer Complete"));
    lv_obj_set_style_text_font(g_timer_ui.finished_title, &lv_font_chicago_8, 0);
    lv_obj_align(g_timer_ui.finished_title, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *countdown = lv_label_create(g_timer_ui.finished_panel);
    lv_label_set_text(countdown, "00:00");
    lv_obj_set_style_text_font(countdown, &lv_font_chicago_48, 0);
    lv_obj_align(countdown, LV_ALIGN_TOP_MID, 0, 25);

    lv_obj_t *dismiss =
        CreateButton(g_timer_ui.finished_panel, tr("Dismiss"), DismissEvent);
    g_timer_ui.dismiss_label = lv_obj_get_child(dismiss, 0);
    lv_obj_set_size(dismiss, 260, 52);
    lv_obj_align(dismiss, LV_ALIGN_TOP_MID, 0, 91);

    g_timer_ui.finished_help = lv_label_create(g_timer_ui.finished_panel);
    lv_label_set_text(
        g_timer_ui.finished_help,
        tr("Press Clock or Alarm to dismiss"));
    lv_obj_set_style_text_font(g_timer_ui.finished_help, &lv_font_chicago_8, 0);
    lv_obj_align(g_timer_ui.finished_help, LV_ALIGN_BOTTOM_MID, 0, 0);

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
    g_last_ui_seconds = UINT32_MAX;
    if (timer_is_active())
    {
        SetCountdownSeconds(timer_remaining_seconds(now_ms));
        lv_label_set_text(g_timer_ui.status, tr("Running in background"));
        lv_obj_clear_flag(
            g_timer_ui.stop_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(
            g_timer_ui.adjustments, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        SetCountdownSeconds(
            (uint32_t)g_selected_minutes * 60U);
        lv_label_set_text(g_timer_ui.status, tr("Adjust duration"));
        lv_obj_add_flag(
            g_timer_ui.stop_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(
            g_timer_ui.adjustments, LV_OBJ_FLAG_HIDDEN);
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

void timer_ui_refresh_language()
{
    if (!g_timer_ui.panel)
        return;
    lv_label_set_text(g_timer_ui.title, tr("Timer"));
    lv_label_set_text(g_timer_ui.start_label, tr("Start"));
    lv_label_set_text(g_timer_ui.stop_label, tr("Stop"));
    lv_label_set_text(g_timer_ui.back_label, tr("Back"));
    lv_label_set_text(g_timer_ui.finished_title, tr("Timer Complete"));
    lv_label_set_text(g_timer_ui.dismiss_label, tr("Dismiss"));
    lv_label_set_text(
        g_timer_ui.finished_help,
        tr("Press Clock or Alarm to dismiss"));
    lv_label_set_text(
        g_timer_ui.status,
        tr(timer_is_active()
               ? "Running in background"
               : "Adjust duration"));
}
