#ifdef MACLOCK_COMBINED_SOURCE
static uint8_t configured_display_hour(uint8_t hour)
{
    if (g_time_format.hour_format == HourFormat::Hour24)
        return hour;
    const uint8_t hour12 = hour % 12;
    return hour12 ? hour12 : 12;
}

static const char *configured_meridiem(uint8_t hour)
{
    return hour < 12 ? "AM" : "PM";
}

static void format_configured_time(
    const DateTime &current, char *text, size_t text_size)
{
    snprintf(text, text_size, "%02u:%02u",
        configured_display_hour(current.hour()), current.minute());
}

static lv_obj_t *create_clock_face_root(
    lv_obj_t *screen, lv_color_t color)
{
    lv_obj_t *root = lv_obj_create(screen);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, color, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
    return root;
}

static lv_obj_t *create_clock_face_label(
    lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}
#endif
