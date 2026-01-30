#include "datetime_ui.h"

LV_FONT_DECLARE(lv_font_chicago_8);
LV_FONT_DECLARE(lv_font_chicago_32);

extern RTC_DS1307 rtc;
extern void request_state(int state);

struct DateTimeUi
{
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *time_row;
    lv_obj_t *date_row;
    lv_obj_t *hour;
    lv_obj_t *minute;
    lv_obj_t *second;
    lv_obj_t *day;
    lv_obj_t *month;
    lv_obj_t *year;
    lv_obj_t *plus_btn;
    lv_obj_t *minus_btn;
    lv_obj_t *save_btn;
    lv_obj_t *cancel_btn;
    lv_obj_t *active_spinbox;
};

static DateTimeUi g_dt_ui = {};
static lv_obj_t *g_dt_spinboxes[6] = {};
static uint8_t g_dt_spinbox_digits[6] = {};
static int32_t g_dt_spinbox_min[6] = {};
static int32_t g_dt_spinbox_max[6] = {};
static int g_dt_spinbox_count = 0;
static lv_style_t g_spinbox_style;
static lv_style_t g_spinbox_style_active;
static lv_style_t g_btn_style;
static lv_style_t g_btn_style_pressed;

static void select_last_digit(lv_obj_t *spinbox)
{
    for (int i = 0; i < g_dt_spinbox_count; ++i)
    {
        if (g_dt_spinboxes[i] == spinbox)
        {
            lv_spinbox_set_step(spinbox, 1);
            lv_spinbox_set_cursor_pos(spinbox, g_dt_spinbox_digits[i] - 1);
            break;
        }
    }
}

static void spinbox_focus_event(lv_event_t *e)
{
    lv_obj_t *spinbox = (lv_obj_t *)lv_event_get_user_data(e);
    if (!spinbox)
        spinbox = (lv_obj_t *)lv_event_get_target(e);
    g_dt_ui.active_spinbox = spinbox;
    for (int i = 0; i < g_dt_spinbox_count; ++i)
        lv_obj_clear_state(g_dt_spinboxes[i], LV_STATE_CHECKED);
    if (g_dt_ui.active_spinbox)
        lv_obj_add_state(g_dt_ui.active_spinbox, LV_STATE_CHECKED);

    if (g_dt_ui.active_spinbox)
        select_last_digit(g_dt_ui.active_spinbox);
}

static void plus_event(lv_event_t *e)
{
    (void)e;
    if (!g_dt_ui.active_spinbox)
        return;

    for (int i = 0; i < g_dt_spinbox_count; ++i)
    {
        if (g_dt_spinboxes[i] == g_dt_ui.active_spinbox)
        {
            int32_t value = lv_spinbox_get_value(g_dt_ui.active_spinbox);
            int32_t max_value = g_dt_spinbox_max[i];
            if (value < max_value)
                lv_spinbox_set_value(g_dt_ui.active_spinbox, value + 1);
            break;
        }
    }
    select_last_digit(g_dt_ui.active_spinbox);
}

static void minus_event(lv_event_t *e)
{
    (void)e;
    if (!g_dt_ui.active_spinbox)
        return;

    for (int i = 0; i < g_dt_spinbox_count; ++i)
    {
        if (g_dt_spinboxes[i] == g_dt_ui.active_spinbox)
        {
            int32_t value = lv_spinbox_get_value(g_dt_ui.active_spinbox);
            int32_t min_value = g_dt_spinbox_min[i];
            if (value > min_value)
                lv_spinbox_set_value(g_dt_ui.active_spinbox, value - 1);
            break;
        }
    }
    select_last_digit(g_dt_ui.active_spinbox);
}

static void datetime_save_event(lv_event_t *e)
{
    (void)e;
    int year = lv_spinbox_get_value(g_dt_ui.year);
    int month = lv_spinbox_get_value(g_dt_ui.month);
    int day = lv_spinbox_get_value(g_dt_ui.day);
    int hour = lv_spinbox_get_value(g_dt_ui.hour);
    int minute = lv_spinbox_get_value(g_dt_ui.minute);
    int second = lv_spinbox_get_value(g_dt_ui.second);
    rtc.adjust(DateTime(year, month, day, hour, minute, second));
    request_state(7);
}

static void datetime_cancel_event(lv_event_t *e)
{
    (void)e;
    request_state(7);
}

static lv_obj_t *create_spinbox_column(lv_obj_t *parent, int min_value, int max_value, int digits, int init_value)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, 0, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 2, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_flex_grow(col, 1);

    lv_obj_t *spinbox = lv_spinbox_create(col);
    lv_obj_remove_style_all(spinbox);
    lv_spinbox_set_range(spinbox, min_value, max_value);
    lv_spinbox_set_digit_format(spinbox, digits, 0);
    lv_spinbox_set_value(spinbox, init_value);
    lv_spinbox_set_rollover(spinbox, true);
    lv_spinbox_set_step(spinbox, 1);
    lv_obj_set_size(spinbox, lv_pct(100), 30);
    lv_obj_set_style_text_font(spinbox, &lv_font_chicago_8, LV_PART_MAIN);
    lv_obj_set_style_text_font(spinbox, &lv_font_chicago_8, LV_PART_SELECTED);
    lv_obj_set_style_text_font(spinbox, &lv_font_chicago_8, LV_PART_ITEMS);
    lv_obj_set_style_text_letter_space(spinbox, 0, 0);
    lv_obj_set_style_text_line_space(spinbox, 0, 0);
    lv_obj_set_style_text_align(spinbox, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(spinbox, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(spinbox, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_left(spinbox, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_right(spinbox, 2, LV_PART_MAIN);
    lv_obj_add_style(spinbox, &g_spinbox_style, LV_PART_MAIN);
    lv_obj_add_style(spinbox, &g_spinbox_style_active, LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(spinbox, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(spinbox, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_set_style_text_color(spinbox, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_opa(spinbox, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(spinbox, lv_color_black(), LV_PART_SELECTED);
    lv_obj_set_style_text_opa(spinbox, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_text_color(spinbox, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_text_opa(spinbox, LV_OPA_COVER, LV_PART_ITEMS);
    lv_textarea_set_text_selection(spinbox, false);
    lv_obj_set_scrollbar_mode(spinbox, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(spinbox, spinbox_focus_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(spinbox, spinbox_focus_event, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(spinbox, spinbox_focus_event, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(col, spinbox_focus_event, LV_EVENT_PRESSED, spinbox);
    lv_obj_add_event_cb(col, spinbox_focus_event, LV_EVENT_CLICKED, spinbox);

    return spinbox;
}

void datetime_ui_init(lv_obj_t *scr)
{
    lv_style_init(&g_spinbox_style);
    lv_style_set_bg_color(&g_spinbox_style, lv_color_white());
    lv_style_set_bg_opa(&g_spinbox_style, LV_OPA_COVER);
    lv_style_set_border_color(&g_spinbox_style, lv_color_black());
    lv_style_set_border_width(&g_spinbox_style, 1);
    lv_style_set_text_color(&g_spinbox_style, lv_color_black());
    lv_style_set_text_opa(&g_spinbox_style, LV_OPA_COVER);
    lv_style_set_pad_all(&g_spinbox_style, 2);

    lv_style_init(&g_spinbox_style_active);
    lv_style_set_bg_color(&g_spinbox_style_active, lv_color_black());
    lv_style_set_bg_opa(&g_spinbox_style_active, LV_OPA_COVER);
    lv_style_set_text_color(&g_spinbox_style_active, lv_color_white());
    lv_style_set_border_color(&g_spinbox_style_active, lv_color_black());
    lv_style_set_border_width(&g_spinbox_style_active, 2);

    lv_style_init(&g_btn_style);
    lv_style_set_bg_color(&g_btn_style, lv_color_white());
    lv_style_set_bg_opa(&g_btn_style, LV_OPA_COVER);
    lv_style_set_border_color(&g_btn_style, lv_color_black());
    lv_style_set_border_width(&g_btn_style, 2);
    lv_style_set_text_color(&g_btn_style, lv_color_black());
    lv_style_set_pad_all(&g_btn_style, 2);

    lv_style_init(&g_btn_style_pressed);
    lv_style_set_bg_color(&g_btn_style_pressed, lv_color_black());
    lv_style_set_bg_opa(&g_btn_style_pressed, LV_OPA_COVER);
    lv_style_set_text_color(&g_btn_style_pressed, lv_color_white());
    lv_style_set_border_color(&g_btn_style_pressed, lv_color_black());
    lv_style_set_border_width(&g_btn_style_pressed, 2);

    g_dt_ui.panel = lv_obj_create(scr);
    lv_obj_set_size(g_dt_ui.panel, 280, 190);
    lv_obj_center(g_dt_ui.panel);
    lv_obj_set_style_bg_color(g_dt_ui.panel, lv_color_white(), 0);
    lv_obj_set_style_border_color(g_dt_ui.panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_dt_ui.panel, 2, 0);
    lv_obj_set_style_pad_all(g_dt_ui.panel, 8, 0);
    lv_obj_set_flex_flow(g_dt_ui.panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_dt_ui.panel, 6, 0);

    g_dt_ui.title = lv_label_create(g_dt_ui.panel);
    lv_label_set_text(g_dt_ui.title, "Set Date/Time");
    lv_obj_set_style_text_font(g_dt_ui.title, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_letter_space(g_dt_ui.title, 1, 0);
    lv_obj_set_style_text_align(g_dt_ui.title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(g_dt_ui.title, lv_pct(100));

    g_dt_ui.time_row = lv_obj_create(g_dt_ui.panel);
    lv_obj_remove_style_all(g_dt_ui.time_row);
    lv_obj_set_size(g_dt_ui.time_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g_dt_ui.time_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(g_dt_ui.time_row, 8, 0);
    lv_obj_set_style_pad_row(g_dt_ui.time_row, 0, 0);
    lv_obj_set_style_pad_all(g_dt_ui.time_row, 0, 0);
    lv_obj_set_style_align(g_dt_ui.time_row, LV_ALIGN_CENTER, 0);

    g_dt_ui.hour = create_spinbox_column(g_dt_ui.time_row, 0, 23, 2, 0);
    g_dt_spinboxes[g_dt_spinbox_count++] = g_dt_ui.hour;
    g_dt_spinbox_digits[g_dt_spinbox_count - 1] = 2;
    g_dt_spinbox_min[g_dt_spinbox_count - 1] = 0;
    g_dt_spinbox_max[g_dt_spinbox_count - 1] = 23;
    lv_obj_t *time_sep1 = lv_label_create(g_dt_ui.time_row);
    lv_label_set_text(time_sep1, ":");
    lv_obj_set_style_text_font(time_sep1, &lv_font_chicago_8, 0);

    g_dt_ui.minute = create_spinbox_column(g_dt_ui.time_row, 0, 59, 2, 0);
    g_dt_spinboxes[g_dt_spinbox_count++] = g_dt_ui.minute;
    g_dt_spinbox_digits[g_dt_spinbox_count - 1] = 2;
    g_dt_spinbox_min[g_dt_spinbox_count - 1] = 0;
    g_dt_spinbox_max[g_dt_spinbox_count - 1] = 59;
    lv_obj_t *time_sep2 = lv_label_create(g_dt_ui.time_row);
    lv_label_set_text(time_sep2, ":");
    lv_obj_set_style_text_font(time_sep2, &lv_font_chicago_8, 0);

    g_dt_ui.second = create_spinbox_column(g_dt_ui.time_row, 0, 59, 2, 0);
    g_dt_spinboxes[g_dt_spinbox_count++] = g_dt_ui.second;
    g_dt_spinbox_digits[g_dt_spinbox_count - 1] = 2;
    g_dt_spinbox_min[g_dt_spinbox_count - 1] = 0;
    g_dt_spinbox_max[g_dt_spinbox_count - 1] = 59;

    g_dt_ui.date_row = lv_obj_create(g_dt_ui.panel);
    lv_obj_remove_style_all(g_dt_ui.date_row);
    lv_obj_set_size(g_dt_ui.date_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g_dt_ui.date_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(g_dt_ui.date_row, 8, 0);
    lv_obj_set_style_pad_row(g_dt_ui.date_row, 0, 0);
    lv_obj_set_style_pad_all(g_dt_ui.date_row, 0, 0);
    lv_obj_set_style_align(g_dt_ui.date_row, LV_ALIGN_CENTER, 0);

    g_dt_ui.day = create_spinbox_column(g_dt_ui.date_row, 1, 31, 2, 1);
    g_dt_spinboxes[g_dt_spinbox_count++] = g_dt_ui.day;
    g_dt_spinbox_digits[g_dt_spinbox_count - 1] = 2;
    g_dt_spinbox_min[g_dt_spinbox_count - 1] = 1;
    g_dt_spinbox_max[g_dt_spinbox_count - 1] = 31;
    lv_obj_t *date_sep1 = lv_label_create(g_dt_ui.date_row);
    lv_label_set_text(date_sep1, "/");
    lv_obj_set_style_text_font(date_sep1, &lv_font_chicago_8, 0);

    g_dt_ui.month = create_spinbox_column(g_dt_ui.date_row, 1, 12, 2, 1);
    g_dt_spinboxes[g_dt_spinbox_count++] = g_dt_ui.month;
    g_dt_spinbox_digits[g_dt_spinbox_count - 1] = 2;
    g_dt_spinbox_min[g_dt_spinbox_count - 1] = 1;
    g_dt_spinbox_max[g_dt_spinbox_count - 1] = 12;
    lv_obj_t *date_sep2 = lv_label_create(g_dt_ui.date_row);
    lv_label_set_text(date_sep2, "/");
    lv_obj_set_style_text_font(date_sep2, &lv_font_chicago_8, 0);

    g_dt_ui.year = create_spinbox_column(g_dt_ui.date_row, 2000, 2099, 4, 2026);
    g_dt_spinboxes[g_dt_spinbox_count++] = g_dt_ui.year;
    g_dt_spinbox_digits[g_dt_spinbox_count - 1] = 4;
    g_dt_spinbox_min[g_dt_spinbox_count - 1] = 2000;
    g_dt_spinbox_max[g_dt_spinbox_count - 1] = 2099;

    lv_obj_t *step_row = lv_obj_create(g_dt_ui.panel);
    lv_obj_remove_style_all(step_row);
    lv_obj_set_size(step_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(step_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(step_row, 12, 0);
    lv_obj_set_style_align(step_row, LV_ALIGN_CENTER, 0);
    lv_obj_set_flex_align(step_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    g_dt_ui.minus_btn = lv_btn_create(step_row);
    lv_obj_set_size(g_dt_ui.minus_btn, 120, 32);
    lv_obj_add_style(g_dt_ui.minus_btn, &g_btn_style, 0);
    lv_obj_add_style(g_dt_ui.minus_btn, &g_btn_style_pressed, LV_STATE_PRESSED);
    lv_obj_t *minus_lbl = lv_label_create(g_dt_ui.minus_btn);
    lv_label_set_text(minus_lbl, "-");
    lv_obj_center(minus_lbl);
    lv_obj_set_style_text_font(minus_lbl, &lv_font_chicago_32, 0);
    lv_obj_add_event_cb(g_dt_ui.minus_btn, minus_event, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(g_dt_ui.minus_btn, minus_event, LV_EVENT_LONG_PRESSED_REPEAT, NULL);

    g_dt_ui.plus_btn = lv_btn_create(step_row);
    lv_obj_set_size(g_dt_ui.plus_btn, 120, 32);
    lv_obj_add_style(g_dt_ui.plus_btn, &g_btn_style, 0);
    lv_obj_add_style(g_dt_ui.plus_btn, &g_btn_style_pressed, LV_STATE_PRESSED);
    lv_obj_t *plus_lbl = lv_label_create(g_dt_ui.plus_btn);
    lv_label_set_text(plus_lbl, "+");
    lv_obj_center(plus_lbl);
    lv_obj_set_style_text_font(plus_lbl, &lv_font_chicago_32, 0);
    lv_obj_add_event_cb(g_dt_ui.plus_btn, plus_event, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(g_dt_ui.plus_btn, plus_event, LV_EVENT_LONG_PRESSED_REPEAT, NULL);

    lv_obj_t *btn_row = lv_obj_create(g_dt_ui.panel);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_row, 12, 0);
    lv_obj_set_style_align(btn_row, LV_ALIGN_CENTER, 0);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    g_dt_ui.save_btn = lv_btn_create(btn_row);
    lv_obj_set_size(g_dt_ui.save_btn, 120, 32);
    lv_obj_add_style(g_dt_ui.save_btn, &g_btn_style, 0);
    lv_obj_add_style(g_dt_ui.save_btn, &g_btn_style_pressed, LV_STATE_PRESSED);
    lv_obj_t *save_lbl = lv_label_create(g_dt_ui.save_btn);
    lv_label_set_text(save_lbl, "Save");
    lv_obj_center(save_lbl);
    lv_obj_set_style_text_font(save_lbl, &lv_font_chicago_8, 0);
    lv_obj_add_event_cb(g_dt_ui.save_btn, datetime_save_event, LV_EVENT_CLICKED, NULL);

    g_dt_ui.cancel_btn = lv_btn_create(btn_row);
    lv_obj_set_size(g_dt_ui.cancel_btn, 120, 32);
    lv_obj_add_style(g_dt_ui.cancel_btn, &g_btn_style, 0);
    lv_obj_add_style(g_dt_ui.cancel_btn, &g_btn_style_pressed, LV_STATE_PRESSED);
    lv_obj_t *cancel_lbl = lv_label_create(g_dt_ui.cancel_btn);
    lv_label_set_text(cancel_lbl, "Cancel");
    lv_obj_center(cancel_lbl);
    lv_obj_set_style_text_font(cancel_lbl, &lv_font_chicago_8, 0);
    lv_obj_add_event_cb(g_dt_ui.cancel_btn, datetime_cancel_event, LV_EVENT_CLICKED, NULL);

    lv_obj_add_flag(g_dt_ui.panel, LV_OBJ_FLAG_HIDDEN);
}

void datetime_ui_hide()
{
    if (g_dt_ui.panel)
        lv_obj_add_flag(g_dt_ui.panel, LV_OBJ_FLAG_HIDDEN);
}

void datetime_ui_show()
{
    if (g_dt_ui.panel)
        lv_obj_clear_flag(g_dt_ui.panel, LV_OBJ_FLAG_HIDDEN);
}

void datetime_ui_enter(const DateTime &current)
{
    if (!g_dt_ui.panel)
        return;

    lv_spinbox_set_value(g_dt_ui.hour, current.hour());
    lv_spinbox_set_value(g_dt_ui.minute, current.minute());
    lv_spinbox_set_value(g_dt_ui.second, current.second());
    lv_spinbox_set_value(g_dt_ui.day, current.day());
    lv_spinbox_set_value(g_dt_ui.month, current.month());
    lv_spinbox_set_value(g_dt_ui.year, current.year());
    g_dt_ui.active_spinbox = g_dt_ui.hour;
    for (int i = 0; i < g_dt_spinbox_count; ++i)
        lv_obj_clear_state(g_dt_spinboxes[i], LV_STATE_CHECKED);
    lv_obj_add_state(g_dt_ui.active_spinbox, LV_STATE_CHECKED);
    select_last_digit(g_dt_ui.active_spinbox);
}
