#include "timer_ui.h"
#include "audio_volume.h"
#include "localization.h"

#include <Arduino.h>
#include <stdio.h>

LV_FONT_DECLARE(lv_font_chicago_8);
LV_FONT_DECLARE(lv_font_chicago_48);

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

} // namespace

struct TimerService::State
{
    Preferences *preferences = nullptr;
    bool active = false;
    bool finished_pending = false;
    uint32_t end_ms = 0;
    uint32_t last_ui_seconds = UINT32_MAX;
    uint16_t selected_minutes = 25;
    char sound_path[SOUND_SELECTOR_PATH_MAX] = "/quack.mp3";
    uint8_t volume = kDefaultAudioVolumeIndex;
    TimerUi ui = {};
    TimerView *view = nullptr;
    AppEventSink *events = nullptr;
};

namespace
{
TimerService *active_timer_service = nullptr;

#define g_timer_active (active_timer_service->state().active)
#define g_timer_preferences \
    (active_timer_service->state().preferences)
#define g_timer_finished_pending \
    (active_timer_service->state().finished_pending)
#define g_timer_end_ms (active_timer_service->state().end_ms)
#define g_last_ui_seconds \
    (active_timer_service->state().last_ui_seconds)
#define g_selected_minutes \
    (active_timer_service->state().selected_minutes)
#define g_timer_sound_path \
    (active_timer_service->state().sound_path)
#define g_timer_volume (active_timer_service->state().volume)
#define g_timer_ui (active_timer_service->state().ui)
#define g_timer_view (active_timer_service->state().view)
#define g_events (active_timer_service->state().events)

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
    active_timer_service->start(g_selected_minutes);
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
    if (g_timer_active)
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
    if (g_events)
        g_events->requestState(UiState::Normal);
}

static void StopEvent(lv_event_t *event)
{
    (void)event;
    g_timer_active = false;
    g_timer_finished_pending = false;
    if (g_timer_view)
        g_timer_view->enter(millis());
}

static void BackEvent(lv_event_t *event)
{
    (void)event;
    if (g_events)
        g_events->requestState(UiState::Normal);
}

static void DismissEvent(lv_event_t *event)
{
    (void)event;
    if (g_events)
        g_events->dismissTimer();
}

static void InitTimerPanel(lv_obj_t *screen)
{
    g_timer_ui.panel = lv_obj_create(screen);
    lv_obj_remove_flag(
        g_timer_ui.panel, LV_OBJ_FLAG_SCROLLABLE);
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
    lv_obj_remove_flag(
        g_timer_ui.finished_panel, LV_OBJ_FLAG_SCROLLABLE);
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

TimerService::State &TimerService::state()
{
    if (!state_)
        state_ = new State();
    return *state_;
}

void TimerService::begin(Preferences &preferences)
{
    active_timer_service = this;
    state();
    g_timer_preferences = &preferences;
    const uint16_t saved_minutes =
        preferences.getUShort("timer_minutes", 25);
    g_selected_minutes =
        saved_minutes >= 1 && saved_minutes <= 1440
            ? saved_minutes
            : 25;
    const uint8_t saved_volume =
        preferences.getUChar(
            "timer_volume", kDefaultAudioVolumeIndex);
    g_timer_volume =
        saved_volume < kAudioVolumeLevelCount
            ? saved_volume
            : kDefaultAudioVolumeIndex;
    const String saved_sound =
        preferences.getString("timer_sound", "/quack.mp3");
    strlcpy(
        g_timer_sound_path, saved_sound.c_str(),
        sizeof(g_timer_sound_path));
}

void TimerService::update(uint32_t now_ms)
{
    if (g_timer_active &&
        (int32_t)(now_ms - g_timer_end_ms) >= 0)
    {
        g_timer_active = false;
        g_timer_finished_pending = true;
    }
}

bool TimerService::takeFinished()
{
    if (!g_timer_finished_pending)
        return false;
    g_timer_finished_pending = false;
    return true;
}

bool TimerService::active() const
{
    return g_timer_active;
}

uint32_t TimerService::remainingSeconds(uint32_t now_ms) const
{
    if (!g_timer_active ||
        (int32_t)(now_ms - g_timer_end_ms) >= 0)
    {
        return 0;
    }

    const uint32_t remaining_ms = g_timer_end_ms - now_ms;
    return (remaining_ms + 999U) / 1000U;
}

void TimerService::formatRemaining(
    uint32_t now_ms, char *text, size_t text_size) const
{
    FormatSeconds(remainingSeconds(now_ms), text, text_size);
}

void TimerService::start(uint16_t minutes)
{
    if (minutes < 1 || minutes > 1440)
        return;
    g_selected_minutes = minutes;
    const uint32_t duration_ms =
        static_cast<uint32_t>(minutes) * 60U * 1000U;
    g_timer_end_ms = millis() + duration_ms;
    g_timer_active = true;
    g_timer_finished_pending = false;
}

void TimerService::cancel()
{
    g_timer_active = false;
    g_timer_finished_pending = false;
    g_timer_end_ms = 0;
}

bool TimerService::configure(
    uint16_t minutes, const char *sound_path, uint8_t volume)
{
    if (minutes < 1 || minutes > 1440 ||
        !sound_path || !sound_path[0] ||
        volume >= kAudioVolumeLevelCount)
    {
        return false;
    }

    g_selected_minutes = minutes;
    g_timer_volume = volume;
    strlcpy(
        g_timer_sound_path,
        SoundSelector::resolvePath(sound_path, "/quack.mp3"),
        sizeof(g_timer_sound_path));
    if (g_timer_preferences)
    {
        g_timer_preferences->putUShort(
            "timer_minutes", g_selected_minutes);
        g_timer_preferences->putUChar(
            "timer_volume", g_timer_volume);
        g_timer_preferences->putString(
            "timer_sound", g_timer_sound_path);
    }
    return true;
}

uint16_t TimerService::selectedMinutes() const
{
    return g_selected_minutes;
}

const char *TimerService::soundPath() const
{
    return SoundSelector::resolvePath(
        g_timer_sound_path, "/quack.mp3");
}

uint8_t TimerService::volume() const
{
    return audio_volume_from_index(g_timer_volume);
}

uint8_t TimerService::volumeIndex() const
{
    return g_timer_volume < kAudioVolumeLevelCount
               ? g_timer_volume
               : kDefaultAudioVolumeIndex;
}

void TimerView::begin(lv_obj_t *screen, AppEventSink &events)
{
    active_timer_service = &service_;
    service_.state();
    g_timer_view = this;
    g_events = &events;
    InitTimerPanel(screen);
    InitFinishedPanel(screen);
}

void TimerView::hide()
{
    if (g_timer_ui.panel)
        lv_obj_add_flag(g_timer_ui.panel, LV_OBJ_FLAG_HIDDEN);
    if (g_timer_ui.finished_panel)
        lv_obj_add_flag(g_timer_ui.finished_panel, LV_OBJ_FLAG_HIDDEN);
}

void TimerView::enter(uint32_t now_ms)
{
    g_last_ui_seconds = UINT32_MAX;
    if (service_.active())
    {
        SetCountdownSeconds(service_.remainingSeconds(now_ms));
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

void TimerView::show(uint32_t now_ms)
{
    if (!g_timer_ui.panel)
        return;

    if (service_.active())
    {
        SetCountdownSeconds(service_.remainingSeconds(now_ms));
    }
    else
    {
        SetCountdownSeconds(
            (uint32_t)g_selected_minutes * 60U);
    }
    lv_obj_clear_flag(g_timer_ui.panel, LV_OBJ_FLAG_HIDDEN);
}

void TimerView::showFinished()
{
    if (g_timer_ui.finished_panel)
    {
        lv_obj_clear_flag(
            g_timer_ui.finished_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

void TimerView::refreshLanguage()
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
        tr(service_.active()
               ? "Running in background"
               : "Adjust duration"));
}
