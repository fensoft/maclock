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

static const lv_font_t *configured_date_font(bool timer_active)
{
    return g_time_format.show_weekday && !timer_active
               ? &lv_font_chicago_24
               : &lv_font_chicago_32;
}

static void format_configured_time(
    const DateTime &current, char *text, size_t text_size)
{
    const uint8_t hour =
        configured_display_hour(current.hour());
    if (g_time_format.show_seconds)
    {
        snprintf(
            text, text_size,
            g_time_format.leading_zero
                ? "%02u:%02u:%02u"
                : "%u:%02u:%02u",
            hour, current.minute(), current.second());
    }
    else
    {
        snprintf(
            text, text_size,
            g_time_format.leading_zero
                ? "%02u:%02u"
                : "%u:%02u",
            hour, current.minute());
    }
}

void ClockView::updateMacintoshLabels(
    const ClockRenderSnapshot &snapshot)
{
    const DateTime &now = snapshot.current;
    int sec = now.second();
    if (sec == last_second)
        return;
    last_second = sec;

    char buf[24];
    const bool hour12 =
        g_time_format.hour_format == HourFormat::Hour12;
    if (g_time_format.show_seconds)
    {
        snprintf(
            buf, sizeof(buf), "%02u:%02u:%02u",
            configured_display_hour(now.hour()),
            now.minute(), sec);
    }
    else
    {
        snprintf(
            buf, sizeof(buf), "%02u:%02u",
            configured_display_hour(now.hour()),
            now.minute());
    }
    lv_label_set_text(ui_shell.time, buf);
    lv_obj_align(
        ui_shell.time, LV_ALIGN_TOP_MID,
        hour12 ? -6 : 0, 14 + 4);
    if (hour12)
    {
        lv_label_set_text(
            ui_shell.time_meridiem,
            now.hour() < 12 ? "A\nM" : "P\nM");
        lv_obj_update_layout(ui_shell.time);
        lv_obj_align_to(
            ui_shell.time_meridiem, ui_shell.time,
            LV_ALIGN_OUT_RIGHT_MID, 4, 1);
        lv_obj_clear_flag(
            ui_shell.time_meridiem,
            LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(
            ui_shell.time_meridiem,
            LV_OBJ_FLAG_HIDDEN);
    }

    format_display_date(now, buf, sizeof(buf));
    lv_label_set_text(ui_shell.date, buf);

    const WifiModeSnapshot &online = snapshot.online;
    const WeatherReading &sensor = snapshot.sensor;
    const bool sensor_valid = sensor.valid;
    const float temperature = sensor.temperature;
    const float gauge_value = sensor.gauge_value;
    const float gauge_min = sensor.gauge_min;
    const float gauge_max = sensor.gauge_max;
    if (sensor.valid)
    {
        switch (sensor.condition)
        {
        case WeatherCondition::Rainy:
            lv_image_set_src(ui_shell.gauge_icon, "S:/rainy.png");
            break;
        case WeatherCondition::Sunny:
            lv_image_set_src(ui_shell.gauge_icon, "S:/sunny.png");
            break;
        default:
            lv_image_set_src(ui_shell.gauge_icon, "S:/cloudy.png");
            break;
        }
    }

    if (online.forecast_valid)
    {
        char internal[16];
        if (sensor_valid)
            snprintf(
                internal, sizeof(internal), "%.1f°",
                display_temperature(temperature));
        else
            snprintf(internal, sizeof(internal), "--");

        char weather[112];
        snprintf(
            weather, sizeof(weather),
            "%s: %s   %s: %.1f°   %s: %.0f-%.0f°%c",
            tr("In"), internal, tr("Out"),
            display_temperature(online.current_temperature),
            tr("Today"),
            display_temperature(online.minimum_temperature),
            display_temperature(online.maximum_temperature),
            display_temperature_unit());

        lv_label_set_text(ui_shell.temp, weather);
        lv_obj_clear_flag(ui_shell.temp, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(
            ui_shell.date,
            configured_date_font(snapshot.timer_active), 0);
        lv_obj_align(ui_shell.date, LV_ALIGN_TOP_MID,
                     0, 14 + 4 + 32 + 16);
        lv_obj_set_style_text_font(
            ui_shell.temp, &lv_font_chicago_8, 0);
        lv_obj_set_style_text_letter_space(ui_shell.temp, 0, 0);
        lv_label_set_long_mode(ui_shell.temp, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(ui_shell.temp, 236);
        lv_obj_set_style_text_align(
            ui_shell.temp, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(
            ui_shell.temp, LV_ALIGN_BOTTOM_LEFT, 0, -3);
        lv_obj_align(
            ui_shell.gauge_icon, LV_ALIGN_BOTTOM_RIGHT, -12, -3);

        if (online.weather_code <= 1)
            lv_image_set_src(ui_shell.gauge_icon, "S:/sunny.png");
        else if (online.weather_code <= 3 ||
                 online.weather_code == 45 ||
                 online.weather_code == 48)
            lv_image_set_src(ui_shell.gauge_icon, "S:/cloudy.png");
        else
            lv_image_set_src(ui_shell.gauge_icon, "S:/rainy.png");
        lv_obj_add_flag(ui_shell.gauge_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_shell.gauge_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(ui_shell.temp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(
        ui_shell.date,
        configured_date_font(snapshot.timer_active), 0);
    lv_obj_set_style_text_font(
        ui_shell.temp, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_letter_space(ui_shell.temp, 1, 0);
    lv_obj_align(ui_shell.date, LV_ALIGN_TOP_MID,
                 0, 14 + 4 + 32 + 16);
    lv_obj_set_width(ui_shell.temp, 220);
    lv_obj_set_style_text_align(
        ui_shell.temp, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(ui_shell.temp, LV_ALIGN_TOP_LEFT, 12, 118);
    lv_obj_align(
        ui_shell.gauge_icon, LV_ALIGN_TOP_RIGHT, -12, 111);

    if (!sensor_valid)
    {
        lv_label_set_text(ui_shell.temp, "--");
        lv_obj_add_flag(ui_shell.gauge_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_shell.gauge_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(ui_shell.gauge_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_shell.gauge_box, LV_OBJ_FLAG_HIDDEN);

    char tbuf[12];
    snprintf(
        tbuf, sizeof(tbuf), "%02.1f°%c",
        display_temperature(temperature),
        display_temperature_unit());
    lv_label_set_text(ui_shell.temp, tbuf);

    if (gauge_width == 0)
    {
        gauge_width = lv_obj_get_width(ui_shell.gauge_line);
        gauge_box_width = lv_obj_get_width(ui_shell.gauge_box);
    }

    float clamped = gauge_value;
    if (clamped < gauge_min)
        clamped = gauge_min;
    if (clamped > gauge_max)
        clamped = gauge_max;
    const float t = (clamped - gauge_min) / (gauge_max - gauge_min);
    const int16_t max_offset =
        gauge_width - gauge_box_width;
    const int16_t x_offset = (int16_t)(max_offset * t);
    lv_obj_align_to(ui_shell.gauge_box, ui_shell.gauge_line, LV_ALIGN_LEFT_MID, x_offset, 0);
    lv_obj_move_foreground(ui_shell.gauge_box);
}

static void update_alarm_indicator_layout(bool active)
{
    static constexpr int kIndicatorGap = 4;
    static constexpr int kDateShift = (18 + kIndicatorGap) / 2;
    const int date_top = 14 + 4 + 32 + 16;

    lv_obj_align(ui_shell.date, LV_ALIGN_TOP_MID,
                 active ? kDateShift : 0, date_top);
    if (!active)
    {
        lv_obj_add_flag(
            ui_shell.alarm_indicator, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_update_layout(ui_shell.date);
    lv_obj_align_to(ui_shell.alarm_indicator, ui_shell.date,
                    LV_ALIGN_OUT_LEFT_MID, -kIndicatorGap, 0);
    lv_obj_clear_flag(
        ui_shell.alarm_indicator, LV_OBJ_FLAG_HIDDEN);
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
    lv_obj_t *parent, const lv_font_t *font,
    lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static void set_analog_hand(
    lv_obj_t *hand, lv_point_precise_t points[2],
    float angle_degrees, float length, float tail)
{
    static constexpr float kDegreesToRadians =
        3.14159265358979323846f / 180.0f;
    static constexpr float kCenter = 84.0f;
    const float angle = (angle_degrees - 90.0f) *
                        kDegreesToRadians;
    const float x = cosf(angle);
    const float y = sinf(angle);
    points[0].x = (lv_value_precise_t)lroundf(
        kCenter - x * tail);
    points[0].y = (lv_value_precise_t)lroundf(
        kCenter - y * tail);
    points[1].x = (lv_value_precise_t)lroundf(
        kCenter + x * length);
    points[1].y = (lv_value_precise_t)lroundf(
        kCenter + y * length);
    lv_line_set_points_mutable(hand, points, 2);
}

static lv_obj_t *create_flip_card(
    lv_obj_t *parent, int16_t x, int16_t width)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, width, 96);
    lv_obj_set_pos(card, x, 42);
    lv_obj_set_style_bg_color(card, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_black(), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hinge = lv_obj_create(card);
    lv_obj_remove_flag(hinge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style_all(hinge);
    lv_obj_set_size(hinge, lv_pct(100), 1);
    lv_obj_align(hinge, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(hinge, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(hinge, LV_OPA_50, 0);
    return card;
}

static lv_obj_t *create_flip_flap(
    lv_obj_t *card, lv_obj_t **label, int16_t width)
{
    lv_obj_t *flap = lv_obj_create(card);
    lv_obj_remove_style_all(flap);
    lv_obj_set_size(flap, lv_pct(100), lv_pct(100));
    lv_obj_center(flap);
    lv_obj_set_style_bg_color(flap, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(flap, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(flap, lv_color_black(), 0);
    lv_obj_set_style_border_width(flap, 2, 0);
    lv_obj_set_style_radius(flap, 8, 0);
    lv_obj_set_style_transform_pivot_x(
        flap, width / 2, 0);
    lv_obj_set_style_transform_pivot_y(flap, 48, 0);
    lv_obj_set_style_transform_scale_y(
        flap, LV_SCALE_NONE, 0);
    lv_obj_remove_flag(flap, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hinge = lv_obj_create(flap);
    lv_obj_remove_flag(hinge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style_all(hinge);
    lv_obj_set_size(hinge, lv_pct(100), 1);
    lv_obj_align(hinge, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(hinge, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(hinge, LV_OPA_50, 0);

    *label = create_clock_face_label(
        flap, &lv_font_chicago_48, lv_color_white());
    lv_label_set_text(*label, "00");
    lv_obj_center(*label);
    lv_obj_add_flag(flap, LV_OBJ_FLAG_HIDDEN);
    return flap;
}

static void flip_scale_y_animation(
    void *object, int32_t scale)
{
    lv_obj_set_style_transform_scale_y(
        (lv_obj_t *)object, scale, 0);
}

static void flip_expand_completed(lv_anim_t *animation)
{
    FlipCardAnimation *state =
        (FlipCardAnimation *)lv_anim_get_user_data(animation);
    if (!state)
        return;
    strlcpy(
        state->displayed, state->pending,
        sizeof(state->displayed));
    lv_label_set_text(state->label, state->displayed);
    lv_obj_set_style_transform_scale_y(
        state->flap, LV_SCALE_NONE, 0);
    lv_obj_add_flag(state->flap, LV_OBJ_FLAG_HIDDEN);
    state->animating = false;
}

static void flip_collapse_completed(lv_anim_t *animation)
{
    FlipCardAnimation *state =
        (FlipCardAnimation *)lv_anim_get_user_data(animation);
    if (!state)
        return;
    lv_label_set_text(state->flap_label, state->pending);

    lv_anim_t expand;
    lv_anim_init(&expand);
    lv_anim_set_var(&expand, state->flap);
    lv_anim_set_exec_cb(&expand, flip_scale_y_animation);
    lv_anim_set_values(&expand, 12, LV_SCALE_NONE);
    lv_anim_set_duration(&expand, 210);
    lv_anim_set_path_cb(&expand, lv_anim_path_ease_out);
    lv_anim_set_user_data(&expand, state);
    lv_anim_set_completed_cb(
        &expand, flip_expand_completed);
    lv_anim_start(&expand);
}

static void reset_flip_card_animation(
    FlipCardAnimation &state)
{
    if (!state.flap)
        return;
    lv_anim_delete(state.flap, flip_scale_y_animation);
    lv_obj_set_style_transform_scale_y(
        state.flap, LV_SCALE_NONE, 0);
    lv_obj_add_flag(state.flap, LV_OBJ_FLAG_HIDDEN);
    state.initialized = false;
    state.animating = false;
}

void ClockView::applyTimeFormatLayout()
{
    const bool show_seconds = g_time_format.show_seconds;
    const size_t visible_digits = show_seconds ? 6 : 4;
    const int16_t card_width = show_seconds ? 38 : 56;
    static constexpr int16_t compact_positions[4] = {
        22, 82, 166, 226};
    static constexpr int16_t seconds_positions[6] = {
        8, 48, 102, 142, 196, 236};

    for (size_t i = 0; i < kFlipDigitCount; ++i)
    {
        reset_flip_card_animation(
            clock_view.flip_animations[i]);
        if (i >= visible_digits)
        {
            lv_obj_add_flag(
                clock_view.flip_cards[i],
                LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        lv_obj_clear_flag(
            clock_view.flip_cards[i],
            LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(
            clock_view.flip_cards[i],
            card_width, 96);
        lv_obj_set_pos(
            clock_view.flip_cards[i],
            show_seconds
                ? seconds_positions[i]
                : compact_positions[i],
            42);
        lv_obj_set_style_transform_pivot_x(
            clock_view.flip_animations[i].flap,
            card_width / 2, 0);
    }

    const int16_t colon_width = show_seconds ? 10 : 16;
    const int16_t colon_positions[2] = {
        static_cast<int16_t>(show_seconds ? 90 : 144),
        static_cast<int16_t>(show_seconds ? 184 : 0)};
    for (size_t i = 0; i < 2; ++i)
    {
        if (i == 1 && !show_seconds)
        {
            lv_obj_add_flag(
                clock_view.flip_colons[i],
                LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(
            clock_view.flip_colons[i],
            LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(
            clock_view.flip_colons[i],
            colon_width, 96);
        lv_obj_set_pos(
            clock_view.flip_colons[i],
            colon_positions[i], 42);
        const int16_t dot_x = (colon_width - 4) / 2;
        lv_obj_set_x(
            clock_view.flip_colon_tops[i], dot_x);
        lv_obj_set_x(
            clock_view.flip_colon_bottoms[i], dot_x);
    }

    const bool hour12 =
        g_time_format.hour_format == HourFormat::Hour12;
    if (hour12)
    {
        lv_obj_clear_flag(
            clock_view.compact_meridiem,
            LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(
            clock_view.flip_meridiem,
            LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(
            clock_view.compact_meridiem,
            LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(
            clock_view.flip_meridiem,
            LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_flip_card(
    FlipCardAnimation &state, const char *value)
{
    if (!state.initialized)
    {
        strlcpy(
            state.displayed, value,
            sizeof(state.displayed));
        strlcpy(
            state.pending, value,
            sizeof(state.pending));
        lv_label_set_text(state.label, value);
        lv_label_set_text(state.flap_label, value);
        state.initialized = true;
        return;
    }

    if (state.animating)
    {
        if (strcmp(value, state.pending) == 0)
            return;
        lv_anim_delete(state.flap, flip_scale_y_animation);
        lv_obj_set_style_transform_scale_y(
            state.flap, LV_SCALE_NONE, 0);
        lv_obj_add_flag(state.flap, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(state.label, state.displayed);
        state.animating = false;
    }
    if (strcmp(value, state.displayed) == 0)
        return;

    strlcpy(
        state.pending, value, sizeof(state.pending));
    lv_label_set_text(state.label, state.pending);
    lv_label_set_text(state.flap_label, state.displayed);
    lv_obj_set_style_transform_scale_y(
        state.flap, LV_SCALE_NONE, 0);
    lv_obj_clear_flag(state.flap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(state.flap);
    state.animating = true;

    lv_anim_t collapse;
    lv_anim_init(&collapse);
    lv_anim_set_var(&collapse, state.flap);
    lv_anim_set_exec_cb(&collapse, flip_scale_y_animation);
    lv_anim_set_values(&collapse, LV_SCALE_NONE, 12);
    lv_anim_set_duration(&collapse, 170);
    lv_anim_set_path_cb(&collapse, lv_anim_path_ease_in);
    lv_anim_set_user_data(&collapse, &state);
    lv_anim_set_completed_cb(
        &collapse, flip_collapse_completed);
    lv_anim_start(&collapse);
}

void ClockView::applyTheme()
{
    const bool dark = g_clock_theme == CLOCK_THEME_DARK;
    const lv_color_t background =
        dark ? lv_color_black() : lv_color_white();
    const lv_color_t foreground =
        dark ? lv_color_white() : lv_color_black();
    const lv_color_t card_background =
        dark ? lv_color_hex(0x181818) : lv_color_black();
    const lv_color_t card_border =
        dark ? lv_color_hex(0x707070) : lv_color_black();

    lv_obj_set_style_bg_color(
        clock_view.compact, background, 0);
    lv_obj_set_style_text_color(
        clock_view.compact_title, foreground, 0);
    lv_obj_set_style_text_color(
        clock_view.compact_time, foreground, 0);
    lv_obj_set_style_text_color(
        clock_view.compact_meridiem, foreground, 0);
    lv_obj_set_style_text_color(
        clock_view.compact_date, foreground, 0);
    lv_obj_set_style_text_color(
        clock_view.compact_weather, foreground, 0);

    lv_obj_set_style_bg_color(
        clock_view.analog, background, 0);
    lv_obj_set_style_bg_color(
        clock_view.analog_dial, background, 0);
    lv_obj_set_style_border_color(
        clock_view.analog_dial, foreground, 0);
    for (lv_obj_t *number : clock_view.analog_numbers)
        lv_obj_set_style_text_color(number, foreground, 0);
    lv_obj_set_style_line_color(
        clock_view.analog_hour_hand, foreground, 0);
    lv_obj_set_style_line_color(
        clock_view.analog_minute_hand, foreground, 0);
    lv_obj_set_style_line_color(
        clock_view.analog_second_hand, foreground, 0);
    lv_obj_set_style_bg_color(
        clock_view.analog_center, foreground, 0);
    lv_obj_set_style_text_color(
        clock_view.analog_date, foreground, 0);

    lv_obj_set_style_bg_color(
        clock_view.flip, background, 0);
    lv_obj_set_style_text_color(
        clock_view.flip_title, foreground, 0);
    for (size_t i = 0; i < 2; ++i)
    {
        lv_obj_set_style_bg_color(
            clock_view.flip_colon_tops[i],
            foreground, 0);
        lv_obj_set_style_bg_color(
            clock_view.flip_colon_bottoms[i],
            foreground, 0);
    }
    lv_obj_set_style_text_color(
        clock_view.flip_meridiem, foreground, 0);
    lv_obj_set_style_text_color(
        clock_view.flip_date, foreground, 0);

    for (FlipCardAnimation &card :
         clock_view.flip_animations)
    {
        lv_obj_set_style_bg_color(
            card.card, card_background, 0);
        lv_obj_set_style_border_color(
            card.card, card_border, 0);
        lv_obj_set_style_bg_color(
            card.flap, card_background, 0);
        lv_obj_set_style_border_color(
            card.flap, card_border, 0);
        lv_obj_set_style_text_color(
            card.label, lv_color_white(), 0);
        lv_obj_set_style_text_color(
            card.flap_label, lv_color_white(), 0);
    }
}

#endif
