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

static lv_color_t configured_accent_color(
    bool dark_surface, lv_color_t fallback)
{
    switch (g_face_customization.accent)
    {
    case FaceAccent::Red:
        return lv_color_hex(dark_surface ? 0xFF6B6B : 0xC62828);
    case FaceAccent::Orange:
        return lv_color_hex(dark_surface ? 0xFFB347 : 0xB45309);
    case FaceAccent::Green:
        return lv_color_hex(dark_surface ? 0x4ADE80 : 0x15803D);
    case FaceAccent::Blue:
        return lv_color_hex(dark_surface ? 0x60A5FA : 0x1D4ED8);
    case FaceAccent::Purple:
        return lv_color_hex(dark_surface ? 0xC084FC : 0x7E22CE);
    case FaceAccent::Default:
    case FaceAccent::Count:
        return fallback;
    }
    return fallback;
}

static void set_object_visible(lv_obj_t *object, bool visible)
{
    if (!object)
        return;
    if (visible)
        lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
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
    format_configured_time(now, buf, sizeof(buf));
    lv_label_set_text(ui_shell.time, buf);
    const int16_t numeral_top_gap =
        g_face_customization.numeral_size ==
                FaceNumeralSize::Default
            ? 0
            : 3;
    const int16_t numeral_left_offset =
        g_face_customization.numeral_size ==
                FaceNumeralSize::Large
            ? -1
            : 0;
    lv_obj_align(
        ui_shell.time, LV_ALIGN_TOP_MID,
        (hour12 ? -6 : 0) + numeral_left_offset,
        14 + 4 + numeral_top_gap);
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

    if (!g_face_customization.show_weather)
    {
        set_object_visible(ui_shell.temp, false);
        set_object_visible(ui_shell.gauge_icon, false);
        set_object_visible(ui_shell.gauge_line, false);
        set_object_visible(ui_shell.gauge_box, false);
        return;
    }

    const WifiModeSnapshot &online = snapshot.online;
    const WeatherReading &sensor = snapshot.sensor;
    const bool macos8 =
        g_clock_face == CLOCK_FACE_MACOS8;
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
            lv_image_set_src(
                ui_shell.gauge_icon,
                g_clock_face == CLOCK_FACE_MACOS8
                    ? "S:/macos8_rainy.png"
                    : "S:/rainy.png");
            break;
        case WeatherCondition::Sunny:
            lv_image_set_src(
                ui_shell.gauge_icon,
                g_clock_face == CLOCK_FACE_MACOS8
                    ? "S:/macos8_sunny.png"
                    : "S:/sunny.png");
            break;
        default:
            lv_image_set_src(
                ui_shell.gauge_icon,
                g_clock_face == CLOCK_FACE_MACOS8
                    ? "S:/macos8_cloudy.png"
                    : "S:/cloudy.png");
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
        lv_obj_set_width(ui_shell.temp, macos8 ? 224 : 236);
        lv_obj_set_style_text_align(
            ui_shell.temp, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(
            ui_shell.temp, LV_ALIGN_BOTTOM_LEFT,
            macos8 ? 8 : 0, macos8 ? -7 : -3);
        lv_obj_align(
            ui_shell.gauge_icon, LV_ALIGN_BOTTOM_RIGHT,
            macos8 ? -8 : -12, macos8 ? -7 : -3);

        if (online.weather_code <= 1)
            lv_image_set_src(
                ui_shell.gauge_icon,
                g_clock_face == CLOCK_FACE_MACOS8
                    ? "S:/macos8_sunny.png"
                    : "S:/sunny.png");
        else if (online.weather_code <= 3 ||
                 online.weather_code == 45 ||
                 online.weather_code == 48)
            lv_image_set_src(
                ui_shell.gauge_icon,
                g_clock_face == CLOCK_FACE_MACOS8
                    ? "S:/macos8_cloudy.png"
                    : "S:/cloudy.png");
        else
            lv_image_set_src(
                ui_shell.gauge_icon,
                g_clock_face == CLOCK_FACE_MACOS8
                    ? "S:/macos8_rainy.png"
                    : "S:/rainy.png");
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
    if (macos8)
    {
        lv_obj_align(
            ui_shell.temp, LV_ALIGN_BOTTOM_LEFT, 8, -7);
        lv_obj_align(
            ui_shell.gauge_icon,
            LV_ALIGN_BOTTOM_RIGHT, -8, -7);
    }
    else
    {
        lv_obj_align(
            ui_shell.temp, LV_ALIGN_TOP_LEFT, 12, 118);
        lv_obj_align(
            ui_shell.gauge_icon,
            LV_ALIGN_TOP_RIGHT, -12, 111);
    }

    if (!sensor_valid)
    {
        lv_label_set_text(ui_shell.temp, "--");
        lv_obj_add_flag(ui_shell.gauge_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_shell.gauge_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    char tbuf[12];
    snprintf(
        tbuf, sizeof(tbuf), "%02.1f°%c",
        display_temperature(temperature),
        display_temperature_unit());
    lv_label_set_text(ui_shell.temp, tbuf);

    if (macos8)
    {
        lv_obj_add_flag(
            ui_shell.gauge_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(
            ui_shell.gauge_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(ui_shell.gauge_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_shell.gauge_box, LV_OBJ_FLAG_HIDDEN);

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
    lv_obj_t *parent, int16_t x, int16_t width,
    lv_obj_t **left_pin, lv_obj_t **right_pin)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, width, 100);
    lv_obj_set_pos(card, x, 40);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x55544e), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x77766e), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t **pins[2] = {left_pin, right_pin};
    const int16_t pin_positions[2] = {
        1, static_cast<int16_t>(width - 5)};
    for (size_t i = 0; i < 2; ++i)
    {
        lv_obj_t *pin = lv_obj_create(card);
        *pins[i] = pin;
        lv_obj_remove_style_all(pin);
        lv_obj_set_size(pin, 4, 6);
        lv_obj_set_pos(pin, pin_positions[i], 47);
        lv_obj_set_style_bg_color(pin, lv_color_hex(0xb1afa4), 0);
        lv_obj_set_style_bg_opa(pin, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(pin, 2, 0);
        lv_obj_remove_flag(pin, LV_OBJ_FLAG_SCROLLABLE);
    }
    return card;
}

static lv_obj_t *create_flip_half(
    lv_obj_t *card, int16_t y, lv_obj_t **label,
    int16_t width, bool bottom)
{
    lv_obj_t *half = lv_obj_create(card);
    lv_obj_remove_style_all(half);
    lv_obj_set_size(half, width - 4, 47);
    lv_obj_set_pos(half, 2, y);
    lv_obj_set_style_bg_color(
        half,
        bottom ? lv_color_hex(0x101010)
               : lv_color_hex(0x1d1d1d),
        0);
    lv_obj_set_style_bg_opa(half, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        half, lv_color_hex(0x090909), 0);
    lv_obj_set_style_border_width(half, 1, 0);
    lv_obj_set_style_radius(half, 3, 0);
    lv_obj_remove_flag(half, LV_OBJ_FLAG_SCROLLABLE);

    *label = create_clock_face_label(
        half, &lv_font_chicago_48, lv_color_white());
    lv_label_set_text(*label, "0");
    lv_obj_set_width(*label, lv_pct(100));
    lv_obj_set_style_text_align(
        *label, LV_TEXT_ALIGN_CENTER, 0);
    return half;
}

static lv_obj_t *create_flip_flap(
    lv_obj_t *card, int16_t y, lv_obj_t **label,
    int16_t width, bool bottom)
{
    lv_obj_t *flap = create_flip_half(
        card, y, label, width, bottom);
    lv_obj_set_style_transform_pivot_x(
        flap, (width - 4) / 2, 0);
    lv_obj_set_style_transform_pivot_y(
        flap, bottom ? 0 : 47, 0);
    lv_obj_set_style_transform_scale_y(
        flap, LV_SCALE_NONE, 0);
    lv_obj_add_flag(flap, LV_OBJ_FLAG_HIDDEN);
    return flap;
}

static void position_flip_labels(
    FlipCardAnimation &state, const lv_font_t *font)
{
    const int16_t line_height =
        static_cast<int16_t>(lv_font_get_line_height(font));
    const int16_t full_y = (96 - line_height) / 2;
    lv_obj_set_y(state.top_label, full_y);
    lv_obj_set_y(state.top_flap_label, full_y);
    lv_obj_set_y(state.bottom_label, full_y - 49);
    lv_obj_set_y(state.bottom_flap_label, full_y - 49);
}

static void flip_scale_y_animation(
    void *object, int32_t scale)
{
    lv_obj_set_style_transform_scale_y(
        (lv_obj_t *)object, scale, 0);
}

static uint32_t flip_collapse_duration()
{
    switch (g_face_customization.flip_speed)
    {
    case FlipAnimationSpeed::Slow:
        return 260;
    case FlipAnimationSpeed::Fast:
        return 100;
    case FlipAnimationSpeed::Normal:
    case FlipAnimationSpeed::Count:
        return 170;
    }
    return 170;
}

static uint32_t flip_expand_duration()
{
    switch (g_face_customization.flip_speed)
    {
    case FlipAnimationSpeed::Slow:
        return 320;
    case FlipAnimationSpeed::Fast:
        return 130;
    case FlipAnimationSpeed::Normal:
    case FlipAnimationSpeed::Count:
        return 210;
    }
    return 210;
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
    lv_label_set_text(
        state->top_label, state->displayed);
    lv_label_set_text(
        state->bottom_label, state->displayed);
    lv_obj_set_style_transform_scale_y(
        state->bottom_flap, LV_SCALE_NONE, 0);
    lv_obj_add_flag(
        state->bottom_flap, LV_OBJ_FLAG_HIDDEN);
    state->animating = false;
}

static void flip_collapse_completed(lv_anim_t *animation)
{
    FlipCardAnimation *state =
        (FlipCardAnimation *)lv_anim_get_user_data(animation);
    if (!state)
        return;
    lv_obj_add_flag(
        state->top_flap, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(
        state->bottom_flap_label, state->pending);
    lv_obj_clear_flag(
        state->bottom_flap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(state->bottom_flap);

    lv_anim_t expand;
    lv_anim_init(&expand);
    lv_anim_set_var(&expand, state->bottom_flap);
    lv_anim_set_exec_cb(&expand, flip_scale_y_animation);
    lv_anim_set_values(&expand, 12, LV_SCALE_NONE);
    lv_anim_set_duration(&expand, flip_expand_duration());
    lv_anim_set_path_cb(&expand, lv_anim_path_ease_out);
    lv_anim_set_user_data(&expand, state);
    lv_anim_set_completed_cb(
        &expand, flip_expand_completed);
    lv_anim_start(&expand);
}

static void reset_flip_card_animation(
    FlipCardAnimation &state)
{
    if (!state.top_flap)
        return;
    lv_anim_delete(
        state.top_flap, flip_scale_y_animation);
    lv_anim_delete(
        state.bottom_flap, flip_scale_y_animation);
    lv_obj_set_style_transform_scale_y(
        state.top_flap, LV_SCALE_NONE, 0);
    lv_obj_set_style_transform_scale_y(
        state.bottom_flap, LV_SCALE_NONE, 0);
    lv_obj_add_flag(
        state.top_flap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(
        state.bottom_flap, LV_OBJ_FLAG_HIDDEN);
    state.initialized = false;
    state.animating = false;
}

static void reset_odometer_digit_animation(
    OdometerDigitAnimation &state);
static void update_odometer_digit(
    OdometerDigitAnimation &state, const char *value);

void ClockView::applyTimeFormatLayout()
{
    const bool show_seconds = g_time_format.show_seconds;
    const size_t visible_digits = show_seconds ? 6 : 4;
    const int16_t card_width = show_seconds ? 38 : 56;
    static constexpr int16_t compact_positions[4] = {
        22, 82, 166, 226};
    static constexpr int16_t seconds_positions[6] = {
        19, 59, 113, 153, 207, 247};

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
            card_width, 100);
        lv_obj_set_pos(
            clock_view.flip_cards[i],
            show_seconds
                ? seconds_positions[i]
                : compact_positions[i],
            40);
        lv_obj_set_width(
            clock_view.flip_animations[i].top_panel,
            card_width - 4);
        lv_obj_set_width(
            clock_view.flip_animations[i].bottom_panel,
            card_width - 4);
        lv_obj_set_width(
            clock_view.flip_animations[i].top_flap,
            card_width - 4);
        lv_obj_set_width(
            clock_view.flip_animations[i].bottom_flap,
            card_width - 4);
        lv_obj_set_style_transform_pivot_x(
            clock_view.flip_animations[i].top_flap,
            (card_width - 4) / 2, 0);
        lv_obj_set_style_transform_pivot_x(
            clock_view.flip_animations[i].bottom_flap,
            (card_width - 4) / 2, 0);
        lv_obj_set_x(
            clock_view.flip_animations[i].left_pin, 1);
        lv_obj_set_x(
            clock_view.flip_animations[i].right_pin,
            card_width - 5);
    }

    static constexpr int16_t odometer_four_positions[4] = {
        54, 99, 144, 189};
    for (size_t i = 0; i < kOdometerDigitCount; ++i)
    {
        reset_odometer_digit_animation(
            clock_view.odometer_animations[i]);
        if (i >= visible_digits)
        {
            lv_obj_add_flag(
                clock_view.odometer_animations[i].window,
                LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(
            clock_view.odometer_animations[i].window,
            LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(
            clock_view.odometer_animations[i].window,
            show_seconds
                ? 9 + static_cast<int16_t>(i) * 45
                : odometer_four_positions[i]);
    }

    const int16_t colon_width = show_seconds ? 10 : 16;
    const int16_t colon_positions[2] = {
        static_cast<int16_t>(show_seconds ? 101 : 144),
        static_cast<int16_t>(show_seconds ? 195 : 0)};
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

    set_object_visible(
        clock_view.analog_second_hand,
        g_time_format.show_seconds);
}

static void odometer_y_animation(
    void *object, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)object, value);
}

static uint32_t odometer_animation_duration()
{
    switch (g_face_customization.flip_speed)
    {
    case FlipAnimationSpeed::Slow:
        return 500;
    case FlipAnimationSpeed::Fast:
        return 160;
    case FlipAnimationSpeed::Normal:
    case FlipAnimationSpeed::Count:
        return 280;
    }
    return 280;
}

static void odometer_roll_completed(lv_anim_t *animation)
{
    OdometerDigitAnimation *state =
        (OdometerDigitAnimation *)
            lv_anim_get_user_data(animation);
    if (state == nullptr)
        return;
    lv_anim_delete(
        state->current_label, odometer_y_animation);
    strlcpy(
        state->displayed, state->pending,
        sizeof(state->displayed));
    lv_label_set_text(
        state->current_label, state->displayed);
    lv_label_set_text(
        state->next_label, state->displayed);
    lv_obj_set_y(state->current_label, 8);
    lv_obj_set_y(state->next_label, 80);
    lv_obj_move_foreground(state->next_label);
    state->animating = false;
}

static void reset_odometer_digit_animation(
    OdometerDigitAnimation &state)
{
    lv_anim_delete(
        state.current_label, odometer_y_animation);
    lv_anim_delete(
        state.next_label, odometer_y_animation);
    lv_obj_set_y(state.current_label, 8);
    lv_obj_set_y(state.next_label, 80);
    state.initialized = false;
    state.animating = false;
}

static void update_odometer_digit(
    OdometerDigitAnimation &state, const char *value)
{
    if (!state.initialized)
    {
        strlcpy(
            state.displayed, value,
            sizeof(state.displayed));
        strlcpy(
            state.pending, value,
            sizeof(state.pending));
        lv_label_set_text(state.current_label, value);
        state.initialized = true;
        return;
    }
    if (state.animating)
    {
        if (strcmp(value, state.pending) == 0)
            return;
        lv_anim_delete(
            state.current_label, odometer_y_animation);
        lv_anim_delete(
            state.next_label, odometer_y_animation);
        strlcpy(
            state.displayed, state.pending,
            sizeof(state.displayed));
        lv_label_set_text(
            state.current_label, state.displayed);
        lv_obj_set_y(state.current_label, 8);
        lv_obj_set_y(state.next_label, 80);
        state.animating = false;
    }
    if (strcmp(value, state.displayed) == 0)
        return;

    strlcpy(
        state.pending, value, sizeof(state.pending));
    lv_label_set_text(state.next_label, value);
    lv_obj_set_y(state.current_label, 8);
    lv_obj_set_y(state.next_label, 80);
    lv_obj_move_foreground(state.next_label);
    state.animating = true;

    lv_anim_t outgoing;
    lv_anim_init(&outgoing);
    lv_anim_set_var(&outgoing, state.current_label);
    lv_anim_set_exec_cb(&outgoing, odometer_y_animation);
    lv_anim_set_values(&outgoing, 8, -64);
    lv_anim_set_duration(
        &outgoing, odometer_animation_duration());
    lv_anim_set_path_cb(
        &outgoing, lv_anim_path_ease_in_out);
    lv_anim_start(&outgoing);

    lv_anim_t incoming;
    lv_anim_init(&incoming);
    lv_anim_set_var(&incoming, state.next_label);
    lv_anim_set_exec_cb(&incoming, odometer_y_animation);
    lv_anim_set_values(&incoming, 80, 8);
    lv_anim_set_duration(
        &incoming, odometer_animation_duration());
    lv_anim_set_path_cb(
        &incoming, lv_anim_path_ease_in_out);
    lv_anim_set_user_data(&incoming, &state);
    lv_anim_set_completed_cb(
        &incoming, odometer_roll_completed);
    lv_anim_start(&incoming);
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
        lv_label_set_text(state.top_label, value);
        lv_label_set_text(state.bottom_label, value);
        lv_label_set_text(state.top_flap_label, value);
        lv_label_set_text(state.bottom_flap_label, value);
        state.initialized = true;
        return;
    }

    if (state.animating)
    {
        if (strcmp(value, state.pending) == 0)
            return;
        lv_anim_delete(
            state.top_flap, flip_scale_y_animation);
        lv_anim_delete(
            state.bottom_flap, flip_scale_y_animation);
        lv_obj_set_style_transform_scale_y(
            state.top_flap, LV_SCALE_NONE, 0);
        lv_obj_set_style_transform_scale_y(
            state.bottom_flap, LV_SCALE_NONE, 0);
        lv_obj_add_flag(
            state.top_flap, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(
            state.bottom_flap, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(
            state.top_label, state.displayed);
        lv_label_set_text(
            state.bottom_label, state.displayed);
        state.animating = false;
    }
    if (strcmp(value, state.displayed) == 0)
        return;

    strlcpy(
        state.pending, value, sizeof(state.pending));
    lv_label_set_text(state.top_label, state.pending);
    lv_label_set_text(state.bottom_label, state.displayed);
    lv_label_set_text(
        state.top_flap_label, state.displayed);
    lv_label_set_text(
        state.bottom_flap_label, state.pending);
    lv_obj_set_style_transform_scale_y(
        state.top_flap, LV_SCALE_NONE, 0);
    lv_obj_set_style_transform_scale_y(
        state.bottom_flap, 12, 0);
    lv_obj_clear_flag(
        state.top_flap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(
        state.bottom_flap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(state.top_flap);
    state.animating = true;

    lv_anim_t collapse;
    lv_anim_init(&collapse);
    lv_anim_set_var(&collapse, state.top_flap);
    lv_anim_set_exec_cb(&collapse, flip_scale_y_animation);
    lv_anim_set_values(&collapse, LV_SCALE_NONE, 12);
    lv_anim_set_duration(
        &collapse, flip_collapse_duration());
    lv_anim_set_path_cb(&collapse, lv_anim_path_ease_in);
    lv_anim_set_user_data(&collapse, &state);
    lv_anim_set_completed_cb(
        &collapse, flip_collapse_completed);
    lv_anim_start(&collapse);
}

void ClockView::applyTheme()
{
    const bool dark = g_clock_theme == CLOCK_THEME_DARK;
    if (ui_shell.background)
    {
        ui_shell.applyMacintoshAppearance(
            g_clock_face == CLOCK_FACE_MACOS8);
    }
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

    lv_obj_set_style_bg_color(
        clock_view.odometer,
        dark ? lv_color_hex(0x080808)
             : lv_color_hex(0xd8d4c8), 0);
    lv_obj_set_style_text_color(
        clock_view.odometer_title,
        dark ? lv_color_white() : lv_color_black(), 0);
    lv_obj_set_style_text_color(
        clock_view.odometer_meridiem,
        dark ? lv_color_white() : lv_color_black(), 0);
    lv_obj_set_style_text_color(
        clock_view.odometer_date,
        dark ? lv_color_white() : lv_color_black(), 0);

    for (FlipCardAnimation &card :
         clock_view.flip_animations)
    {
        lv_obj_set_style_bg_color(
            card.card, card_background, 0);
        lv_obj_set_style_border_color(
            card.card, card_border, 0);
        lv_obj_set_style_bg_color(
            card.top_panel, lv_color_hex(0x1d1d1d), 0);
        lv_obj_set_style_bg_color(
            card.top_flap, lv_color_hex(0x1d1d1d), 0);
        lv_obj_set_style_bg_color(
            card.bottom_panel, lv_color_hex(0x101010), 0);
        lv_obj_set_style_bg_color(
            card.bottom_flap, lv_color_hex(0x101010), 0);
        for (lv_obj_t *label :
             {card.top_label, card.bottom_label,
              card.top_flap_label,
              card.bottom_flap_label})
            lv_obj_set_style_text_color(
                label, lv_color_white(), 0);
    }

    for (OdometerDigitAnimation &digit :
         clock_view.odometer_animations)
    {
        for (lv_obj_t *label :
            {digit.current_label, digit.next_label})
            lv_obj_set_style_text_color(
                label, lv_color_white(), 0);
    }

    applyFaceCustomization();
}

void ClockView::applyFaceCustomization()
{
    const lv_font_t *time_font = &lv_font_chicago_48;
    const lv_font_t *analog_font = &lv_font_chicago_8;
    switch (g_face_customization.numeral_size)
    {
    case FaceNumeralSize::Small:
        time_font = &lv_font_chicago_digits_40;
        analog_font = &lv_font_chicago_digits_6;
        break;
    case FaceNumeralSize::Large:
        time_font = &lv_font_chicago_digits_56;
        analog_font = &lv_font_chicago_digits_10;
        break;
    case FaceNumeralSize::Default:
    case FaceNumeralSize::Count:
        break;
    }

    lv_obj_set_style_text_font(ui_shell.time, time_font, 0);
    lv_obj_set_style_text_letter_space(
        ui_shell.time,
        g_face_customization.numeral_size ==
                FaceNumeralSize::Large
            ? -1
            : 1,
        0);
    lv_obj_set_style_text_font(
        clock_view.compact_time, time_font, 0);
    for (lv_obj_t *number : clock_view.analog_numbers)
        lv_obj_set_style_text_font(number, analog_font, 0);
    for (FlipCardAnimation &card :
         clock_view.flip_animations)
    {
        for (lv_obj_t *label :
             {card.top_label, card.bottom_label,
              card.top_flap_label,
              card.bottom_flap_label})
            lv_obj_set_style_text_font(label, time_font, 0);
        position_flip_labels(card, time_font);
        reset_flip_card_animation(card);
    }
    for (OdometerDigitAnimation &digit :
         clock_view.odometer_animations)
    {
        for (lv_obj_t *label :
             {digit.current_label, digit.next_label})
            lv_obj_set_style_text_font(label, time_font, 0);
        reset_odometer_digit_animation(digit);
    }

    const bool dark =
        g_clock_theme == CLOCK_THEME_DARK;
    const lv_color_t face_foreground =
        dark ? lv_color_white() : lv_color_black();
    const bool macos8 =
        g_clock_face == CLOCK_FACE_MACOS8;
    lv_obj_set_style_text_color(
        ui_shell.time,
        configured_accent_color(
            macos8 ? false : dark,
            macos8 ? lv_color_black() : face_foreground),
        0);
    lv_obj_set_style_text_color(
        clock_view.compact_time,
        configured_accent_color(dark, face_foreground), 0);
    const lv_color_t analog_accent =
        configured_accent_color(dark, face_foreground);
    lv_obj_set_style_line_color(
        clock_view.analog_second_hand, analog_accent, 0);
    lv_obj_set_style_bg_color(
        clock_view.analog_center, analog_accent, 0);

    const lv_color_t flip_digit_accent =
        configured_accent_color(true, lv_color_white());
    for (FlipCardAnimation &card :
         clock_view.flip_animations)
    {
        for (lv_obj_t *label :
             {card.top_label, card.bottom_label,
              card.top_flap_label,
              card.bottom_flap_label})
            lv_obj_set_style_text_color(
                label, flip_digit_accent, 0);
    }
    for (OdometerDigitAnimation &digit :
         clock_view.odometer_animations)
    {
        for (lv_obj_t *label :
             {digit.current_label, digit.next_label})
            lv_obj_set_style_text_color(
                label, flip_digit_accent, 0);
    }
    const lv_color_t flip_face_accent =
        configured_accent_color(dark, face_foreground);
    for (size_t i = 0; i < 2; ++i)
    {
        lv_obj_set_style_bg_color(
            clock_view.flip_colon_tops[i],
            flip_face_accent, 0);
        lv_obj_set_style_bg_color(
            clock_view.flip_colon_bottoms[i],
            flip_face_accent, 0);
    }

    set_object_visible(
        clock_view.compact_weather,
        g_face_customization.show_weather);
    set_object_visible(
        clock_view.analog_second_hand,
        g_time_format.show_seconds);
}

#endif
