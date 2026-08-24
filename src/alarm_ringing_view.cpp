#include "alarm_ui_internal.h"

#include "localization.h"
#include "sound_selector.h"

LV_FONT_DECLARE(lv_font_chicago_8);
LV_FONT_DECLARE(lv_font_chicago_48);

namespace
{
constexpr uint32_t kAlarmSunriseDurationMs = 60000;

void button_visual_event(lv_event_t *event)
{
    lv_obj_t *label = static_cast<lv_obj_t *>(lv_event_get_user_data(event));
    if (label)
        lv_obj_set_style_text_color(label, lv_event_get_code(event) == LV_EVENT_PRESSED ? lv_color_white() : lv_color_black(), 0);
}

void snooze_event(lv_event_t *)
{
    if (g_events)
        g_events->snoozeActiveAlarm();
}

void dismiss_event(lv_event_t *)
{
    if (g_events)
        g_events->dismissActiveAlarm();
}

lv_obj_t *create_button(lv_obj_t *parent, const char *text, lv_event_cb_t callback)
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
    lv_obj_add_event_cb(button, button_visual_event, LV_EVENT_PRESSED, label);
    lv_obj_add_event_cb(button, button_visual_event, LV_EVENT_RELEASED, label);
    lv_obj_add_event_cb(button, button_visual_event, LV_EVENT_PRESS_LOST, label);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
    return button;
}
}

void alarm_ringing_begin(lv_obj_t *screen)
{
    g_ringing.panel = lv_obj_create(screen);
    lv_obj_remove_flag(g_ringing.panel, LV_OBJ_FLAG_SCROLLABLE);
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
    lv_obj_t *snooze = create_button(g_ringing.panel, tr("Snooze 9 min"), snooze_event);
    g_ringing.snooze_label = lv_obj_get_child(snooze, 0);
    lv_obj_set_size(snooze, 260, 42);
    lv_obj_align(snooze, LV_ALIGN_TOP_MID, 0, 91);
    lv_obj_t *dismiss = create_button(g_ringing.panel, tr("Dismiss"), dismiss_event);
    g_ringing.dismiss_label = lv_obj_get_child(dismiss, 0);
    lv_obj_set_size(dismiss, 260, 42);
    lv_obj_align(dismiss, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(g_ringing.panel, LV_OBJ_FLAG_HIDDEN);
}

void alarm_ringing_hide()
{
    if (g_ringing.panel)
        lv_obj_add_flag(g_ringing.panel, LV_OBJ_FLAG_HIDDEN);
}

void alarm_ringing_show(size_t alarm_index)
{
    if (!g_ringing.panel || alarm_index >= kAlarmCount)
        return;
    const AlarmSettings &alarm = g_alarms[alarm_index];
    char title[kAlarmLabelMaxLength + 12];
    char time[8];
    if (alarm.label[0]) strlcpy(title, alarm.label, sizeof(title));
    else snprintf(title, sizeof(title), "%s %u", tr("Alarm"), static_cast<unsigned>(alarm_index) + 1);
    snprintf(time, sizeof(time), "%02u:%02u", static_cast<unsigned>(alarm.hour), static_cast<unsigned>(alarm.minute));
    lv_label_set_text(g_ringing.title, title);
    lv_label_set_text(g_ringing.time, time);
    lv_label_set_text(g_ringing.sound, SoundSelector::displayName(g_alarm_sound_paths[alarm_index]));
    lv_obj_set_style_bg_color(g_ringing.panel, lv_color_white(), 0);
    lv_obj_set_style_text_color(g_ringing.title, lv_color_black(), 0);
    lv_obj_set_style_text_color(g_ringing.time, lv_color_black(), 0);
    lv_obj_set_style_text_color(g_ringing.sound, lv_color_black(), 0);
    lv_obj_clear_flag(g_ringing.panel, LV_OBJ_FLAG_HIDDEN);
}

void alarm_ringing_update(size_t alarm_index, uint32_t elapsed_ms)
{
    if (!g_ringing.panel || alarm_index >= kAlarmCount || !g_alarms[alarm_index].sunrise)
        return;
    const uint32_t clamped = elapsed_ms < kAlarmSunriseDurationMs ? elapsed_ms : kAlarmSunriseDurationMs;
    const uint8_t mix = static_cast<uint8_t>((clamped * 255ULL) / kAlarmSunriseDurationMs);
    const lv_color_t background = lv_color_mix(lv_color_hex(0xFFF4D0), lv_color_hex(0x341000), mix);
    const lv_color_t text = mix < 128 ? lv_color_white() : lv_color_black();
    lv_obj_set_style_bg_color(g_ringing.panel, background, 0);
    lv_obj_set_style_text_color(g_ringing.title, text, 0);
    lv_obj_set_style_text_color(g_ringing.time, text, 0);
    lv_obj_set_style_text_color(g_ringing.sound, text, 0);
}

void alarm_ringing_refresh_language()
{
    if (!g_ringing.panel)
        return;
    lv_label_set_text(g_ringing.snooze_label, tr("Snooze 9 min"));
    lv_label_set_text(g_ringing.dismiss_label, tr("Dismiss"));
}
