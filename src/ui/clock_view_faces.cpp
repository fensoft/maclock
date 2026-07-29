#ifdef MACLOCK_COMBINED_SOURCE
void ClockView::init(lv_obj_t *screen)
{
    clock_view.compact =
        create_clock_face_root(screen, lv_color_white());
    clock_view.compact_title =
        create_clock_face_label(
            clock_view.compact,
            &lv_font_chicago_8, lv_color_black());
    lv_label_set_text(
        clock_view.compact_title, tr("Clock"));
    lv_obj_align(
        clock_view.compact_title,
        LV_ALIGN_TOP_MID, 0, 13);

    clock_view.compact_time =
        create_clock_face_label(
            clock_view.compact,
            &lv_font_chicago_48, lv_color_black());
    lv_label_set_text(
        clock_view.compact_time, "00:00:00");
    lv_obj_set_width(clock_view.compact_time, 300);
    lv_obj_align(
        clock_view.compact_time,
        LV_ALIGN_TOP_MID, 0, 38);

    clock_view.compact_meridiem =
        create_clock_face_label(
            clock_view.compact,
            &lv_font_chicago_8, lv_color_black());
    lv_label_set_text(
        clock_view.compact_meridiem, "AM");
    lv_obj_align(
        clock_view.compact_meridiem,
        LV_ALIGN_TOP_RIGHT, -12, 96);
    lv_obj_add_flag(
        clock_view.compact_meridiem,
        LV_OBJ_FLAG_HIDDEN);

    clock_view.compact_date =
        create_clock_face_label(
            clock_view.compact,
            &lv_font_chicago_32, lv_color_black());
    lv_label_set_text(
        clock_view.compact_date, "00/00/0000");
    lv_obj_set_width(clock_view.compact_date, 300);
    lv_obj_align(
        clock_view.compact_date,
        LV_ALIGN_TOP_MID, 0, 112);

    clock_view.compact_weather =
        create_clock_face_label(
            clock_view.compact,
            &lv_font_chicago_8, lv_color_black());
    lv_label_set_text(
        clock_view.compact_weather, "--");
    lv_obj_set_width(clock_view.compact_weather, 292);
    lv_label_set_long_mode(
        clock_view.compact_weather,
        LV_LABEL_LONG_CLIP);
    lv_obj_align(
        clock_view.compact_weather,
        LV_ALIGN_BOTTOM_MID, 0, -14);

    clock_view.analog =
        create_clock_face_root(screen, lv_color_white());
    clock_view.analog_dial =
        lv_obj_create(clock_view.analog);
    lv_obj_remove_style_all(clock_view.analog_dial);
    lv_obj_set_size(clock_view.analog_dial, 170, 170);
    lv_obj_align(
        clock_view.analog_dial,
        LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_bg_color(
        clock_view.analog_dial, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        clock_view.analog_dial, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        clock_view.analog_dial, lv_color_black(), 0);
    lv_obj_set_style_border_width(
        clock_view.analog_dial, 2, 0);
    lv_obj_set_style_radius(
        clock_view.analog_dial, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(
        clock_view.analog_dial,
        LV_OBJ_FLAG_SCROLLABLE);

    for (size_t i = 0; i < 12; ++i)
    {
        char number[3];
        snprintf(number, sizeof(number), "%u",
                 (unsigned)i + 1);
        lv_obj_t *label = create_clock_face_label(
            clock_view.analog_dial,
            &lv_font_chicago_8, lv_color_black());
        clock_view.analog_numbers[i] = label;
        lv_label_set_text(label, number);
        const float angle =
            (float)(i + 1) * 30.0f *
            3.14159265358979323846f / 180.0f;
        lv_obj_align(
            label, LV_ALIGN_CENTER,
            (int16_t)lroundf(sinf(angle) * 68.0f),
            (int16_t)lroundf(-cosf(angle) * 68.0f));
    }

    clock_view.analog_hour_hand =
        lv_line_create(clock_view.analog_dial);
    lv_obj_set_style_line_color(
        clock_view.analog_hour_hand,
        lv_color_black(), 0);
    lv_obj_set_style_line_width(
        clock_view.analog_hour_hand, 5, 0);
    lv_obj_set_style_line_rounded(
        clock_view.analog_hour_hand, true, 0);

    clock_view.analog_minute_hand =
        lv_line_create(clock_view.analog_dial);
    lv_obj_set_style_line_color(
        clock_view.analog_minute_hand,
        lv_color_black(), 0);
    lv_obj_set_style_line_width(
        clock_view.analog_minute_hand, 3, 0);
    lv_obj_set_style_line_rounded(
        clock_view.analog_minute_hand, true, 0);

    clock_view.analog_second_hand =
        lv_line_create(clock_view.analog_dial);
    lv_obj_set_style_line_color(
        clock_view.analog_second_hand,
        lv_color_black(), 0);
    lv_obj_set_style_line_width(
        clock_view.analog_second_hand, 1, 0);

    set_analog_hand(
        clock_view.analog_hour_hand,
        clock_view.analog_hour_points, 0, 43, 5);
    set_analog_hand(
        clock_view.analog_minute_hand,
        clock_view.analog_minute_points, 0, 61, 6);
    set_analog_hand(
        clock_view.analog_second_hand,
        clock_view.analog_second_points, 0, 68, 8);

    clock_view.analog_center = lv_obj_create(
        clock_view.analog_dial);
    lv_obj_remove_flag(
        clock_view.analog_center, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style_all(clock_view.analog_center);
    lv_obj_set_size(clock_view.analog_center, 8, 8);
    lv_obj_set_style_bg_color(
        clock_view.analog_center, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(
        clock_view.analog_center, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(
        clock_view.analog_center,
        LV_RADIUS_CIRCLE, 0);
    lv_obj_center(clock_view.analog_center);

    clock_view.analog_date =
        create_clock_face_label(
            clock_view.analog,
            &lv_font_chicago_32, lv_color_black());
    lv_label_set_text(
        clock_view.analog_date, "00/00/0000");
    lv_obj_set_width(clock_view.analog_date, 300);
    lv_obj_align(
        clock_view.analog_date,
        LV_ALIGN_BOTTOM_MID, 0, -8);

    clock_view.flip =
        create_clock_face_root(screen, lv_color_white());
    clock_view.flip_title = create_clock_face_label(
        clock_view.flip,
        &lv_font_chicago_8, lv_color_black());
    lv_label_set_text(
        clock_view.flip_title, tr("Clock"));
    lv_obj_align(
        clock_view.flip_title,
        LV_ALIGN_TOP_MID, 0, 13);

    for (size_t i = 0; i < kFlipDigitCount; ++i)
    {
        clock_view.flip_cards[i] =
            create_flip_card(
                clock_view.flip, 0, 40,
                &clock_view.flip_animations[i].left_pin,
                &clock_view.flip_animations[i].right_pin);

        FlipCardAnimation &animation =
            clock_view.flip_animations[i];
        animation.card = clock_view.flip_cards[i];
        animation.top_panel = create_flip_half(
            animation.card, 2, &animation.top_label,
            40, false);
        animation.bottom_panel = create_flip_half(
            animation.card, 51, &animation.bottom_label,
            40, true);
        animation.top_flap = create_flip_flap(
            animation.card, 2,
            &animation.top_flap_label, 40, false);
        animation.bottom_flap = create_flip_flap(
            animation.card, 51,
            &animation.bottom_flap_label, 40, true);
        clock_view.flip_digits[i] =
            animation.top_label;
        position_flip_labels(
            animation, &lv_font_chicago_48);
    }

    for (size_t i = 0; i < 2; ++i)
    {
        clock_view.flip_colons[i] =
            lv_obj_create(clock_view.flip);
        lv_obj_remove_style_all(
            clock_view.flip_colons[i]);
        lv_obj_set_size(
            clock_view.flip_colons[i], 10, 96);
        lv_obj_set_pos(
            clock_view.flip_colons[i], 0, 42);
        lv_obj_remove_flag(
            clock_view.flip_colons[i],
            LV_OBJ_FLAG_SCROLLABLE);

        clock_view.flip_colon_tops[i] =
            lv_obj_create(clock_view.flip_colons[i]);
        lv_obj_remove_flag(
            clock_view.flip_colon_tops[i],
            LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_style_all(
            clock_view.flip_colon_tops[i]);
        lv_obj_set_size(
            clock_view.flip_colon_tops[i], 4, 4);
        lv_obj_set_pos(
            clock_view.flip_colon_tops[i], 3, 35);
        lv_obj_set_style_bg_color(
            clock_view.flip_colon_tops[i],
            lv_color_black(), 0);
        lv_obj_set_style_bg_opa(
            clock_view.flip_colon_tops[i],
            LV_OPA_COVER, 0);
        lv_obj_set_style_radius(
            clock_view.flip_colon_tops[i],
            LV_RADIUS_CIRCLE, 0);

        clock_view.flip_colon_bottoms[i] =
            lv_obj_create(clock_view.flip_colons[i]);
        lv_obj_remove_flag(
            clock_view.flip_colon_bottoms[i],
            LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_style_all(
            clock_view.flip_colon_bottoms[i]);
        lv_obj_set_size(
            clock_view.flip_colon_bottoms[i], 4, 4);
        lv_obj_set_pos(
            clock_view.flip_colon_bottoms[i], 3, 57);
        lv_obj_set_style_bg_color(
            clock_view.flip_colon_bottoms[i],
            lv_color_black(), 0);
        lv_obj_set_style_bg_opa(
            clock_view.flip_colon_bottoms[i],
            LV_OPA_COVER, 0);
        lv_obj_set_style_radius(
            clock_view.flip_colon_bottoms[i],
            LV_RADIUS_CIRCLE, 0);
    }

    clock_view.flip_meridiem =
        create_clock_face_label(
            clock_view.flip,
            &lv_font_chicago_8, lv_color_black());
    lv_label_set_text(
        clock_view.flip_meridiem, "AM");
    lv_obj_align(
        clock_view.flip_meridiem,
        LV_ALIGN_TOP_RIGHT, -12, 14);
    lv_obj_add_flag(
        clock_view.flip_meridiem,
        LV_OBJ_FLAG_HIDDEN);

    clock_view.flip_date =
        create_clock_face_label(
            clock_view.flip,
            &lv_font_chicago_32, lv_color_black());
    lv_label_set_text(
        clock_view.flip_date, "00/00/0000");
    lv_obj_set_width(clock_view.flip_date, 300);
    lv_obj_align(
        clock_view.flip_date,
        LV_ALIGN_BOTTOM_MID, 0, -16);

    clock_view.odometer =
        create_clock_face_root(screen, lv_color_black());
    clock_view.odometer_title =
        create_clock_face_label(
            clock_view.odometer,
            &lv_font_chicago_8, lv_color_white());
    lv_label_set_text(
        clock_view.odometer_title, tr("Odometer"));
    lv_obj_align(
        clock_view.odometer_title,
        LV_ALIGN_TOP_MID, 0, 14);

    clock_view.odometer_panel =
        lv_obj_create(clock_view.odometer);
    lv_obj_remove_flag(
        clock_view.odometer_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(clock_view.odometer_panel, 292, 94);
    lv_obj_align(
        clock_view.odometer_panel,
        LV_ALIGN_TOP_MID, 0, 43);
    lv_obj_set_style_bg_color(
        clock_view.odometer_panel,
        lv_color_hex(0x242424), 0);
    lv_obj_set_style_bg_opa(
        clock_view.odometer_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        clock_view.odometer_panel,
        lv_color_hex(0x777777), 0);
    lv_obj_set_style_border_width(
        clock_view.odometer_panel, 3, 0);
    lv_obj_set_style_radius(
        clock_view.odometer_panel, 8, 0);
    lv_obj_set_style_pad_all(
        clock_view.odometer_panel, 0, 0);

    for (size_t i = 0; i < kOdometerDigitCount; ++i)
    {
        OdometerDigitAnimation &animation =
            clock_view.odometer_animations[i];
        animation.window =
            lv_obj_create(clock_view.odometer_panel);
        lv_obj_remove_flag(
            animation.window, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(animation.window, 40, 72);
        lv_obj_set_pos(
            animation.window,
            9 + static_cast<int16_t>(i) * 45, 8);
        lv_obj_set_style_bg_color(
            animation.window, lv_color_hex(0x080808), 0);
        lv_obj_set_style_bg_opa(
            animation.window, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(
            animation.window, lv_color_hex(0x555555), 0);
        lv_obj_set_style_border_width(
            animation.window, 1, 0);
        lv_obj_set_style_radius(animation.window, 3, 0);
        lv_obj_set_style_pad_all(animation.window, 0, 0);

        animation.current_label =
            create_clock_face_label(
                animation.window,
                &lv_font_chicago_48, lv_color_white());
        animation.next_label =
            create_clock_face_label(
                animation.window,
                &lv_font_chicago_48, lv_color_white());
        for (lv_obj_t *label :
             {animation.current_label,
              animation.next_label})
        {
            lv_obj_set_width(label, 40);
            lv_obj_set_style_text_align(
                label, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_text(label, "0");
        }
        lv_obj_set_pos(animation.current_label, 0, 8);
        lv_obj_set_pos(animation.next_label, 0, 80);
    }

    clock_view.odometer_meridiem =
        create_clock_face_label(
            clock_view.odometer,
            &lv_font_chicago_8, lv_color_white());
    lv_label_set_text(clock_view.odometer_meridiem, "AM");
    lv_obj_align(
        clock_view.odometer_meridiem,
        LV_ALIGN_TOP_RIGHT, -10, 143);
    lv_obj_add_flag(
        clock_view.odometer_meridiem,
        LV_OBJ_FLAG_HIDDEN);

    clock_view.odometer_date =
        create_clock_face_label(
            clock_view.odometer,
            &lv_font_chicago_24, lv_color_white());
    lv_label_set_text(
        clock_view.odometer_date, "00/00/0000");
    lv_obj_set_width(clock_view.odometer_date, 280);
    lv_obj_align(
        clock_view.odometer_date,
        LV_ALIGN_BOTTOM_MID, 0, -18);

    clock_view.initScreensavers(screen);

    clock_view.applyTimeFormatLayout();
    clock_view.applyTheme();
}

void ClockView::show(const ClockRenderSnapshot &snapshot)
{
    ui_shell.hideAll();
    clock_view.screensaver_active = false;

    if (g_clock_face == CLOCK_FACE_MACINTOSH)
    {
        lv_obj_clear_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.white_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.black_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.menu_titles, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.menu_right, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.clock, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.clock_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.time, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.date, LV_OBJ_FLAG_HIDDEN);
        set_object_visible(
            ui_shell.temp,
            g_face_customization.show_weather);
        set_object_visible(
            ui_shell.gauge_icon,
            g_face_customization.show_weather);
        set_object_visible(
            ui_shell.gauge_line,
            g_face_customization.show_weather);
        set_object_visible(
            ui_shell.gauge_box,
            g_face_customization.show_weather);
        update_alarm_indicator_layout(
            snapshot.alarm_indicator);
        lv_obj_clear_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_t *face = nullptr;
    if (g_clock_face == CLOCK_FACE_COMPACT)
        face = clock_view.compact;
    else if (g_clock_face == CLOCK_FACE_ANALOG)
        face = clock_view.analog;
    else if (g_clock_face == CLOCK_FACE_FLIP)
    {
        face = clock_view.flip;
        for (FlipCardAnimation &animation :
             clock_view.flip_animations)
        {
            reset_flip_card_animation(animation);
        }
    }
    else
    {
        face = clock_view.odometer;
        for (OdometerDigitAnimation &animation :
             clock_view.odometer_animations)
            reset_odometer_digit_animation(animation);
    }
    lv_obj_clear_flag(face, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(face);
}

void ClockView::update(const ClockRenderSnapshot &snapshot)
{
    const lv_font_t *date_font =
        configured_date_font(snapshot.timer_active);
    lv_obj_set_style_text_font(
        ui_shell.date, date_font, 0);
    lv_obj_set_style_text_font(
        clock_view.compact_date, date_font, 0);
    lv_obj_set_style_text_font(
        clock_view.analog_date, date_font, 0);
    lv_obj_set_style_text_font(
        clock_view.flip_date, date_font, 0);
    lv_obj_set_style_text_font(
        clock_view.odometer_date, date_font, 0);

    clock_view.updateMacintoshLabels(snapshot);
    if (g_clock_face == CLOCK_FACE_MACINTOSH)
    {
        if (snapshot.timer_active)
            lv_label_set_text(
                ui_shell.date, snapshot.timer_remaining);
        update_alarm_indicator_layout(
            snapshot.alarm_indicator);
        return;
    }

    const DateTime &current = snapshot.current;
    char time_text[16];
    char footer[24];
    if (snapshot.timer_active)
        strlcpy(
            footer, snapshot.timer_remaining, sizeof(footer));
    else
        format_display_date(current, footer, sizeof(footer));

    if (g_clock_face == CLOCK_FACE_COMPACT)
    {
        format_configured_time(
            current, time_text, sizeof(time_text));
        lv_label_set_text(
            clock_view.compact_time, time_text);
        if (g_time_format.hour_format == HourFormat::Hour12)
        {
            lv_label_set_text(
                clock_view.compact_meridiem,
                configured_meridiem(current.hour()));
            lv_obj_clear_flag(
                clock_view.compact_meridiem,
                LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(
                clock_view.compact_meridiem,
                LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(
            clock_view.compact_date, footer);
        lv_label_set_text(
            clock_view.compact_weather,
            lv_label_get_text(ui_shell.temp));
        set_object_visible(
            clock_view.compact_weather,
            g_face_customization.show_weather);
    }
    else if (g_clock_face == CLOCK_FACE_ANALOG)
    {
        const float seconds = (float)current.second();
        const float minutes =
            (float)current.minute() + seconds / 60.0f;
        const float hours =
            (float)(current.hour() % 12) + minutes / 60.0f;
        set_analog_hand(
            clock_view.analog_hour_hand,
            clock_view.analog_hour_points,
            hours * 30.0f, 43, 5);
        set_analog_hand(
            clock_view.analog_minute_hand,
            clock_view.analog_minute_points,
            minutes * 6.0f, 61, 6);
        set_analog_hand(
            clock_view.analog_second_hand,
            clock_view.analog_second_points,
            seconds * 6.0f, 68, 8);
        lv_label_set_text(
            clock_view.analog_date, footer);
    }
    else if (g_clock_face == CLOCK_FACE_FLIP)
    {
        const uint8_t hour =
            configured_display_hour(current.hour());
        snprintf(
            time_text, sizeof(time_text), "%02u%02u%02u",
            hour, current.minute(), current.second());
        if (!g_time_format.leading_zero && hour < 10)
            time_text[0] = ' ';
        const size_t visible_digits =
            g_time_format.show_seconds ? 6 : 4;
        for (size_t i = 0; i < visible_digits; ++i)
        {
            const char digit[2] = {time_text[i], '\0'};
            update_flip_card(
                clock_view.flip_animations[i],
                digit);
        }
        const lv_opa_t colon_opa =
            current.second() % 2 ? LV_OPA_40 : LV_OPA_COVER;
        const size_t visible_colons =
            g_time_format.show_seconds ? 2 : 1;
        for (size_t i = 0; i < visible_colons; ++i)
        {
            lv_obj_set_style_bg_opa(
                clock_view.flip_colon_tops[i],
                colon_opa, 0);
            lv_obj_set_style_bg_opa(
                clock_view.flip_colon_bottoms[i],
                colon_opa, 0);
        }
        if (g_time_format.hour_format == HourFormat::Hour12)
        {
            lv_label_set_text(
                clock_view.flip_meridiem,
                configured_meridiem(current.hour()));
            lv_obj_clear_flag(
                clock_view.flip_meridiem,
                LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(
                clock_view.flip_meridiem,
                LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(
            clock_view.flip_date, footer);
    }
    else
    {
        const uint8_t hour =
            configured_display_hour(current.hour());
        snprintf(
            time_text, sizeof(time_text), "%02u%02u%02u",
            hour, current.minute(), current.second());
        if (!g_time_format.leading_zero && hour < 10)
            time_text[0] = ' ';
        const size_t visible_digits =
            g_time_format.show_seconds ? 6 : 4;
        for (size_t i = 0; i < visible_digits; ++i)
        {
            const char digit[2] = {time_text[i], '\0'};
            update_odometer_digit(
                clock_view.odometer_animations[i], digit);
        }
        if (g_time_format.hour_format == HourFormat::Hour12)
        {
            lv_label_set_text(
                clock_view.odometer_meridiem,
                configured_meridiem(current.hour()));
            lv_obj_clear_flag(
                clock_view.odometer_meridiem,
                LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(
                clock_view.odometer_meridiem,
                LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(
            clock_view.odometer_date, footer);
    }
}

#endif
