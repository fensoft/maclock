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
                clock_view.flip, 0, 40);
        clock_view.flip_digits[i] =
            create_clock_face_label(
                clock_view.flip_cards[i],
                &lv_font_chicago_48, lv_color_white());
        lv_label_set_text(
            clock_view.flip_digits[i], "0");
        lv_obj_center(clock_view.flip_digits[i]);

        FlipCardAnimation &animation =
            clock_view.flip_animations[i];
        animation.card = clock_view.flip_cards[i];
        animation.label = clock_view.flip_digits[i];
        animation.flap = create_flip_flap(
            animation.card, &animation.flap_label,
            40);
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

    clock_view.screensaver =
        create_clock_face_root(screen, lv_color_black());
    for (size_t i = 0; i < kScreensaverStarCount; ++i)
    {
        lv_obj_t *star =
            lv_obj_create(clock_view.screensaver);
        lv_obj_remove_style_all(star);
        const uint8_t size = (i % 5 == 0) ? 2 : 1;
        lv_obj_set_size(star, size, size);
        lv_obj_set_style_bg_color(star, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(star, LV_OPA_COVER, 0);
        clock_view.screensaver_stars[i] = star;
        clock_view.screensaver_star_x[i] =
            (int16_t)((i * 47 + 13) % 304);
        clock_view.screensaver_star_y[i] =
            (int16_t)((i * 83 + 7) % 224);
        clock_view.screensaver_star_speed[i] =
            (uint8_t)(1 + i % 3);
        lv_obj_set_pos(
            star,
            clock_view.screensaver_star_x[i],
            clock_view.screensaver_star_y[i]);
    }

    clock_view.screensaver_clock =
        lv_obj_create(clock_view.screensaver);
    lv_obj_remove_style_all(
        clock_view.screensaver_clock);
    lv_obj_set_size(
        clock_view.screensaver_clock, 144, 66);
    lv_obj_set_style_bg_color(
        clock_view.screensaver_clock,
        lv_color_black(), 0);
    lv_obj_set_style_bg_opa(
        clock_view.screensaver_clock,
        LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        clock_view.screensaver_clock,
        lv_color_white(), 0);
    lv_obj_set_style_border_width(
        clock_view.screensaver_clock, 1, 0);
    lv_obj_set_style_radius(
        clock_view.screensaver_clock, 4, 0);
    lv_obj_remove_flag(
        clock_view.screensaver_clock,
        LV_OBJ_FLAG_SCROLLABLE);

    clock_view.screensaver_time =
        create_clock_face_label(
            clock_view.screensaver_clock,
            &lv_font_chicago_48, lv_color_white());
    lv_label_set_text(
        clock_view.screensaver_time, "00:00");
    lv_obj_center(clock_view.screensaver_time);
    clock_view.screensaver_clock_x = 8;
    clock_view.screensaver_clock_y = 8;
    clock_view.screensaver_clock_dx = 1;
    clock_view.screensaver_clock_dy = 1;
    lv_obj_set_pos(
        clock_view.screensaver_clock,
        clock_view.screensaver_clock_x,
        clock_view.screensaver_clock_y);

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
        lv_obj_clear_flag(ui_shell.temp, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.gauge_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.gauge_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.gauge_box, LV_OBJ_FLAG_HIDDEN);
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
    else
    {
        face = clock_view.flip;
        for (FlipCardAnimation &animation :
             clock_view.flip_animations)
        {
            reset_flip_card_animation(animation);
        }
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
}

void ClockView::showScreensaver()
{
    ui_shell.hideAll();
    clock_view.screensaver_active = true;
    if (g_cursor)
        lv_obj_add_flag(g_cursor, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(
        clock_view.screensaver, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(clock_view.screensaver);
}

void ClockView::updateScreensaver(
    const ClockRenderSnapshot &snapshot)
{
    const unsigned long now_ms = millis();
    const DateTime &current = snapshot.current;
    if (current.second() != screensaver_last_second)
    {
        char time_text[8];
        snprintf(
            time_text, sizeof(time_text), "%02d:%02d",
            current.hour(), current.minute());
        lv_label_set_text(
            clock_view.screensaver_time, time_text);
        screensaver_last_second = current.second();
    }
    if (screensaver_last_move_ms &&
        now_ms - screensaver_last_move_ms < 60)
        return;
    screensaver_last_move_ms = now_ms;

    for (size_t i = 0; i < kScreensaverStarCount; ++i)
    {
        int16_t y =
            clock_view.screensaver_star_y[i] +
            clock_view.screensaver_star_speed[i];
        if (y >= 224)
        {
            y = 0;
            clock_view.screensaver_star_x[i] =
                (int16_t)(
                    (clock_view.screensaver_star_x[i] +
                     73 + i * 11) %
                    304);
        }
        clock_view.screensaver_star_y[i] = y;
        lv_obj_set_pos(
            clock_view.screensaver_stars[i],
            clock_view.screensaver_star_x[i], y);
    }

    int16_t x = clock_view.screensaver_clock_x +
                clock_view.screensaver_clock_dx;
    int16_t y = clock_view.screensaver_clock_y +
                clock_view.screensaver_clock_dy;
    if (x <= 0 || x >= 160)
    {
        clock_view.screensaver_clock_dx =
            -clock_view.screensaver_clock_dx;
        if (x < 0)
            x = 0;
        else if (x > 160)
            x = 160;
    }
    if (y <= 0 || y >= 158)
    {
        clock_view.screensaver_clock_dy =
            -clock_view.screensaver_clock_dy;
        if (y < 0)
            y = 0;
        else if (y > 158)
            y = 158;
    }
    clock_view.screensaver_clock_x = x;
    clock_view.screensaver_clock_y = y;
    lv_obj_set_pos(
        clock_view.screensaver_clock, x, y);
}

#endif
