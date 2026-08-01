#ifdef MACLOCK_COMBINED_SOURCE

namespace
{
static constexpr int16_t kScreensaverWidth = 304;
static constexpr int16_t kScreensaverHeight = 224;
static constexpr unsigned long kRandomRotationMs = 20000;

static lv_obj_t *create_screensaver_layer(lv_obj_t *parent)
{
    lv_obj_t *layer = lv_obj_create(parent);
    lv_obj_remove_style_all(layer);
    lv_obj_set_size(layer, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(layer, 0, 0);
    lv_obj_remove_flag(layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(layer, LV_OBJ_FLAG_HIDDEN);
    return layer;
}

static void set_screensaver_layer_visible(
    lv_obj_t *layer, bool visible)
{
    if (!layer)
        return;
    if (visible)
        lv_obj_clear_flag(layer, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(layer, LV_OBJ_FLAG_HIDDEN);
}

static void fill_matrix_column(char *text, size_t text_size)
{
    static constexpr char kMatrixCharacters[] = "01*+#";
    size_t offset = 0;
    for (uint8_t row = 0; row < 8 && offset + 2 < text_size; ++row)
    {
        text[offset++] =
            kMatrixCharacters[
                random(0, sizeof(kMatrixCharacters) - 1)];
        if (row != 7)
            text[offset++] = '\n';
    }
    text[offset] = '\0';
}

static ScreensaverMode random_screensaver_mode(
    ScreensaverMode previous)
{
    bool has_photos = false;
    File photo_directory = LittleFS.open("/screensaver");
    if (photo_directory && photo_directory.isDirectory())
    {
        for (File file = photo_directory.openNextFile(); file;
             file = photo_directory.openNextFile())
        {
            const String path = file.name();
            const char *name = path.c_str();
            const size_t length = strlen(name);
            if ((length >= 4 &&
                 strcasecmp(name + length - 4, ".jpg") == 0) ||
                (length >= 5 &&
                 strcasecmp(name + length - 5, ".jpeg") == 0))
            {
                has_photos = true;
                break;
            }
        }
        photo_directory.close();
    }
    const uint8_t mode_count =
        static_cast<uint8_t>(ScreensaverMode::Count) - 2;
    ScreensaverMode selected = ScreensaverMode::AfterDark;
    for (uint8_t attempt = 0; attempt < 32; ++attempt)
    {
        const uint8_t selected_index =
            static_cast<uint8_t>(random(0, mode_count));
        selected = selected_index < 6
            ? static_cast<ScreensaverMode>(selected_index + 1)
            : static_cast<ScreensaverMode>(selected_index + 2);
        if (selected != previous &&
            (selected != ScreensaverMode::PhotoSlideshow || has_photos))
            break;
    }
    return selected;
}

static unsigned long screensaver_frame_interval(
    ScreensaverMode mode)
{
    switch (mode)
    {
    case ScreensaverMode::BouncingMac:
        return 45;
    case ScreensaverMode::MatrixRain:
        return 70;
    case ScreensaverMode::Pipes:
        return 75;
    case ScreensaverMode::FlyingClocks:
        return 55;
    default:
        return 60;
    }
}
} // namespace

void ClockView::initScreensavers(lv_obj_t *screen)
{
    screensaver =
        create_clock_face_root(screen, lv_color_black());

    screensaver_star_layer =
        create_screensaver_layer(screensaver);
    for (size_t i = 0; i < kScreensaverStarCount; ++i)
    {
        lv_obj_t *star =
            lv_obj_create(screensaver_star_layer);
        lv_obj_remove_flag(star, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_style_all(star);
        const uint8_t size = (i % 5 == 0) ? 2 : 1;
        lv_obj_set_size(star, size, size);
        lv_obj_set_style_bg_color(
            star, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(star, LV_OPA_COVER, 0);
        screensaver_stars[i] = star;
        screensaver_star_x[i] =
            static_cast<int16_t>((i * 47 + 13) %
                                 kScreensaverWidth);
        screensaver_star_y[i] =
            static_cast<int16_t>((i * 83 + 7) %
                                 kScreensaverHeight);
        screensaver_star_speed[i] =
            static_cast<uint8_t>(1 + i % 3);
        lv_obj_set_pos(
            star,
            screensaver_star_x[i],
            screensaver_star_y[i]);
    }

    screensaver_clock = lv_obj_create(screensaver);
    lv_obj_remove_style_all(screensaver_clock);
    lv_obj_set_size(screensaver_clock, 144, 66);
    lv_obj_set_style_bg_color(
        screensaver_clock, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(
        screensaver_clock, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        screensaver_clock, lv_color_white(), 0);
    lv_obj_set_style_border_width(
        screensaver_clock, 1, 0);
    lv_obj_set_style_radius(screensaver_clock, 4, 0);
    lv_obj_remove_flag(
        screensaver_clock, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(
        screensaver_clock, LV_OBJ_FLAG_HIDDEN);

    screensaver_time =
        create_clock_face_label(
            screensaver_clock,
            &lv_font_chicago_48, lv_color_white());
    lv_label_set_text(screensaver_time, "00:00");
    lv_obj_center(screensaver_time);
    screensaver_clock_x = 8;
    screensaver_clock_y = 8;
    screensaver_clock_dx = 1;
    screensaver_clock_dy = 1;
    lv_obj_set_pos(
        screensaver_clock,
        screensaver_clock_x,
        screensaver_clock_y);

    screensaver_logo_layer =
        create_screensaver_layer(screensaver);
    screensaver_logo =
        lv_obj_create(screensaver_logo_layer);
    lv_obj_remove_flag(
        screensaver_logo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(screensaver_logo, 58, 70);
    lv_obj_set_style_bg_color(
        screensaver_logo, lv_color_hex(0xD8D8D8), 0);
    lv_obj_set_style_bg_opa(
        screensaver_logo, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        screensaver_logo, lv_color_white(), 0);
    lv_obj_set_style_border_width(
        screensaver_logo, 2, 0);
    lv_obj_set_style_radius(screensaver_logo, 5, 0);
    lv_obj_set_style_pad_all(screensaver_logo, 0, 0);

    lv_obj_t *logo_screen = lv_obj_create(screensaver_logo);
    lv_obj_remove_flag(
        logo_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(logo_screen, 44, 34);
    lv_obj_set_pos(logo_screen, 5, 6);
    lv_obj_set_style_bg_color(
        logo_screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        logo_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        logo_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(logo_screen, 2, 0);
    lv_obj_set_style_radius(logo_screen, 3, 0);
    lv_obj_set_style_pad_all(logo_screen, 0, 0);

    lv_obj_t *logo_face =
        create_clock_face_label(
            logo_screen,
            &lv_font_chicago_8, lv_color_black());
    lv_label_set_text(logo_face, ".  .\n ---");
    lv_obj_center(logo_face);

    lv_obj_t *logo_drive = lv_obj_create(screensaver_logo);
    lv_obj_remove_flag(
        logo_drive, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style_all(logo_drive);
    lv_obj_set_size(logo_drive, 20, 4);
    lv_obj_set_pos(logo_drive, 29, 52);
    lv_obj_set_style_bg_color(
        logo_drive, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(
        logo_drive, LV_OPA_COVER, 0);

    lv_obj_t *logo_led = lv_obj_create(screensaver_logo);
    lv_obj_remove_flag(logo_led, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style_all(logo_led);
    lv_obj_set_size(logo_led, 3, 3);
    lv_obj_set_pos(logo_led, 8, 53);
    lv_obj_set_style_bg_color(
        logo_led, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(
        logo_led, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(
        logo_led, LV_RADIUS_CIRCLE, 0);

    screensaver_logo_x = 18;
    screensaver_logo_y = 12;
    screensaver_logo_dx = 2;
    screensaver_logo_dy = 1;
    lv_obj_set_pos(
        screensaver_logo,
        screensaver_logo_x,
        screensaver_logo_y);

    screensaver_matrix_layer =
        create_screensaver_layer(screensaver);
    for (size_t i = 0;
         i < kScreensaverMatrixColumnCount;
         ++i)
    {
        screensaver_matrix_columns[i] =
            create_clock_face_label(
                screensaver_matrix_layer,
                &lv_font_chicago_8,
                lv_color_hex(0x00FF55));
        fill_matrix_column(
            screensaver_matrix_text[i],
            sizeof(screensaver_matrix_text[i]));
        lv_label_set_text(
            screensaver_matrix_columns[i],
            screensaver_matrix_text[i]);
        lv_obj_set_style_text_line_space(
            screensaver_matrix_columns[i], 2, 0);
        screensaver_matrix_y[i] =
            static_cast<int16_t>(-random(0, 220));
        screensaver_matrix_speed[i] =
            static_cast<uint8_t>(1 + i % 3);
        lv_obj_set_pos(
            screensaver_matrix_columns[i],
            static_cast<int16_t>(i * 25 + 3),
            screensaver_matrix_y[i]);
    }

    screensaver_pipe_layer =
        create_screensaver_layer(screensaver);
    for (size_t i = 0;
         i < kScreensaverPipeSegmentCount;
         ++i)
    {
        screensaver_pipe_segments[i] =
            lv_obj_create(screensaver_pipe_layer);
        lv_obj_remove_flag(
            screensaver_pipe_segments[i],
            LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_style_all(
            screensaver_pipe_segments[i]);
        lv_obj_set_style_bg_opa(
            screensaver_pipe_segments[i],
            LV_OPA_COVER, 0);
        lv_obj_set_style_radius(
            screensaver_pipe_segments[i],
            LV_RADIUS_CIRCLE, 0);
        lv_obj_add_flag(
            screensaver_pipe_segments[i],
            LV_OBJ_FLAG_HIDDEN);
    }

    screensaver_flying_layer =
        create_screensaver_layer(screensaver);
    for (size_t i = 0;
         i < kScreensaverFlyingClockCount;
         ++i)
    {
        lv_obj_t *flying_clock =
            lv_obj_create(screensaver_flying_layer);
        screensaver_flying_clocks[i] = flying_clock;
        lv_obj_remove_flag(
            flying_clock, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(flying_clock, 82, 28);
        lv_obj_set_style_bg_color(
            flying_clock, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(
            flying_clock, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(
            flying_clock, lv_color_black(), 0);
        lv_obj_set_style_border_width(
            flying_clock, 1, 0);
        lv_obj_set_style_radius(flying_clock, 3, 0);
        lv_obj_set_style_pad_all(flying_clock, 0, 0);

        screensaver_flying_times[i] =
            create_clock_face_label(
                flying_clock,
                &lv_font_chicago_8,
                lv_color_black());
        lv_label_set_text(
            screensaver_flying_times[i], "00:00");
        lv_obj_center(screensaver_flying_times[i]);
        screensaver_flying_x[i] =
            static_cast<int16_t>(
                (i * 59 + 9) %
                (kScreensaverWidth - 82));
        screensaver_flying_y[i] =
            static_cast<int16_t>(
                (i * 41 + 11) %
                (kScreensaverHeight - 28));
        screensaver_flying_dx[i] =
            static_cast<int8_t>((i & 1) ? -1 : 1);
        screensaver_flying_dy[i] =
            static_cast<int8_t>((i & 2) ? -1 : 1);
        lv_obj_set_pos(
            flying_clock,
            screensaver_flying_x[i],
            screensaver_flying_y[i]);
    }
    initExtendedScreensavers(screensaver);
}

void ClockView::activateScreensaverMode(
    ScreensaverMode mode, bool reset)
{
    if (mode == ScreensaverMode::Random)
    {
        mode = random_screensaver_mode(
            screensaver_active_mode);
        screensaver_random_due_ms =
            millis() + kRandomRotationMs;
    }
    else
    {
        screensaver_random_due_ms = 0;
    }

    if (mode == ScreensaverMode::Off ||
        static_cast<uint8_t>(mode) >=
            static_cast<uint8_t>(ScreensaverMode::Count))
    {
        mode = ScreensaverMode::AfterDark;
    }
    screensaver_active_mode = mode;

    const bool extended = activateExtendedScreensaver(mode, reset);

    set_screensaver_layer_visible(
        screensaver_star_layer,
        mode == ScreensaverMode::AfterDark ||
            mode == ScreensaverMode::Starfield);
    if (mode == ScreensaverMode::AfterDark)
        lv_obj_clear_flag(
            screensaver_clock, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(
            screensaver_clock, LV_OBJ_FLAG_HIDDEN);
    set_screensaver_layer_visible(
        screensaver_logo_layer,
        mode == ScreensaverMode::BouncingMac);
    set_screensaver_layer_visible(
        screensaver_matrix_layer,
        mode == ScreensaverMode::MatrixRain);
    set_screensaver_layer_visible(
        screensaver_pipe_layer,
        mode == ScreensaverMode::Pipes);
    set_screensaver_layer_visible(
        screensaver_flying_layer,
        mode == ScreensaverMode::FlyingClocks);

    if (extended)
        return;

    if (!reset)
        return;
    screensaver_last_move_ms = 0;
    screensaver_last_second = -1;

    if (mode == ScreensaverMode::Pipes)
    {
        screensaver_pipe_index = 0;
        screensaver_pipe_x = kScreensaverWidth / 2;
        screensaver_pipe_y = kScreensaverHeight / 2;
        screensaver_pipe_direction = 0;
        screensaver_pipe_color = 0;
        for (lv_obj_t *segment :
             screensaver_pipe_segments)
        {
            lv_obj_add_flag(
                segment, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ClockView::showScreensaver(ScreensaverMode mode)
{
    ui_shell.hideAll();
    screensaver_active = true;
    if (g_cursor)
        lv_obj_add_flag(g_cursor, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(screensaver, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(screensaver);
    screensaver_snapshot_ms = 0;
    activateScreensaverMode(mode, true);
}

void ClockView::updateScreensaver(
    const ClockRenderSnapshot &snapshot)
{
    const unsigned long now_ms = millis();
    if (screensaver_random_due_ms &&
        static_cast<int32_t>(
            now_ms - screensaver_random_due_ms) >= 0)
    {
        activateScreensaverMode(
            ScreensaverMode::Random, true);
    }

    const DateTime &current = snapshot.current;
    if (current.second() != screensaver_last_second)
    {
        char time_text[8];
        snprintf(
            time_text, sizeof(time_text), "%02u:%02u",
            current.hour(), current.minute());
        lv_label_set_text(screensaver_time, time_text);
        for (size_t i = 0;
             i < kScreensaverFlyingClockCount;
             ++i)
        {
            const uint8_t shifted_hour =
                static_cast<uint8_t>(
                    (current.hour() + i * 3) % 24);
            snprintf(
                time_text, sizeof(time_text), "%02u:%02u",
                shifted_hour, current.minute());
            lv_label_set_text(
                screensaver_flying_times[i],
                time_text);
        }
        screensaver_last_second = current.second();
    }

    if (updateExtendedScreensaver(snapshot))
        return;

    const unsigned long frame_interval =
        screensaver_frame_interval(
            screensaver_active_mode);
    if (screensaver_last_move_ms &&
        now_ms - screensaver_last_move_ms <
            frame_interval)
    {
        return;
    }
    screensaver_last_move_ms = now_ms;

    if (screensaver_active_mode ==
            ScreensaverMode::AfterDark ||
        screensaver_active_mode ==
            ScreensaverMode::Starfield)
    {
        for (size_t i = 0;
             i < kScreensaverStarCount;
             ++i)
        {
            int16_t y =
                screensaver_star_y[i] +
                screensaver_star_speed[i];
            if (y >= kScreensaverHeight)
            {
                y = 0;
                screensaver_star_x[i] =
                    static_cast<int16_t>(
                        (screensaver_star_x[i] +
                         73 + i * 11) %
                        kScreensaverWidth);
            }
            screensaver_star_y[i] = y;
            lv_obj_set_pos(
                screensaver_stars[i],
                screensaver_star_x[i], y);
        }
    }

    if (screensaver_active_mode ==
        ScreensaverMode::AfterDark)
    {
        int16_t x =
            screensaver_clock_x +
            screensaver_clock_dx;
        int16_t y =
            screensaver_clock_y +
            screensaver_clock_dy;
        if (x <= 0 || x >= 160)
        {
            screensaver_clock_dx =
                -screensaver_clock_dx;
            x = constrain(x, 0, 160);
        }
        if (y <= 0 || y >= 158)
        {
            screensaver_clock_dy =
                -screensaver_clock_dy;
            y = constrain(y, 0, 158);
        }
        screensaver_clock_x = x;
        screensaver_clock_y = y;
        lv_obj_set_pos(screensaver_clock, x, y);
        return;
    }

    if (screensaver_active_mode ==
        ScreensaverMode::BouncingMac)
    {
        int16_t x =
            screensaver_logo_x +
            screensaver_logo_dx;
        int16_t y =
            screensaver_logo_y +
            screensaver_logo_dy;
        const int16_t max_x =
            kScreensaverWidth - 58;
        const int16_t max_y =
            kScreensaverHeight - 70;
        if (x <= 0 || x >= max_x)
        {
            screensaver_logo_dx =
                -screensaver_logo_dx;
            x = constrain(x, 0, max_x);
        }
        if (y <= 0 || y >= max_y)
        {
            screensaver_logo_dy =
                -screensaver_logo_dy;
            y = constrain(y, 0, max_y);
        }
        screensaver_logo_x = x;
        screensaver_logo_y = y;
        lv_obj_set_pos(screensaver_logo, x, y);
        return;
    }

    if (screensaver_active_mode ==
        ScreensaverMode::MatrixRain)
    {
        for (size_t i = 0;
             i < kScreensaverMatrixColumnCount;
             ++i)
        {
            int16_t y =
                screensaver_matrix_y[i] +
                screensaver_matrix_speed[i] * 2;
            if (y >= kScreensaverHeight)
            {
                y = static_cast<int16_t>(
                    -random(60, 190));
                fill_matrix_column(
                    screensaver_matrix_text[i],
                    sizeof(screensaver_matrix_text[i]));
                lv_label_set_text(
                    screensaver_matrix_columns[i],
                    screensaver_matrix_text[i]);
            }
            screensaver_matrix_y[i] = y;
            lv_obj_set_y(
                screensaver_matrix_columns[i], y);
        }
        return;
    }

    if (screensaver_active_mode ==
        ScreensaverMode::Pipes)
    {
        static constexpr lv_color_t kPipeColors[] = {
            LV_COLOR_MAKE(0x24, 0xD9, 0x65),
            LV_COLOR_MAKE(0x21, 0xC7, 0xD9),
            LV_COLOR_MAKE(0xF0, 0xD2, 0x32),
            LV_COLOR_MAKE(0xD6, 0x43, 0xC7)};
        lv_obj_t *segment =
            screensaver_pipe_segments[
                screensaver_pipe_index];
        const bool horizontal =
            (screensaver_pipe_direction & 1) == 0;
        lv_obj_set_size(
            segment,
            horizontal ? 16 : 4,
            horizontal ? 4 : 16);
        lv_obj_set_pos(
            segment,
            screensaver_pipe_x,
            screensaver_pipe_y);
        lv_obj_set_style_bg_color(
            segment,
            kPipeColors[
                screensaver_pipe_color %
                (sizeof(kPipeColors) /
                 sizeof(kPipeColors[0]))],
            0);
        lv_obj_clear_flag(
            segment, LV_OBJ_FLAG_HIDDEN);
        screensaver_pipe_index =
            static_cast<uint8_t>(
                (screensaver_pipe_index + 1) %
                kScreensaverPipeSegmentCount);

        int16_t next_x = screensaver_pipe_x;
        int16_t next_y = screensaver_pipe_y;
        switch (screensaver_pipe_direction)
        {
        case 0:
            next_x += 12;
            break;
        case 1:
            next_y += 12;
            break;
        case 2:
            next_x -= 12;
            break;
        default:
            next_y -= 12;
            break;
        }
        const bool boundary =
            next_x < 4 ||
            next_x > kScreensaverWidth - 18 ||
            next_y < 4 ||
            next_y > kScreensaverHeight - 18;
        if (boundary || random(0, 6) == 0)
        {
            const int8_t turn =
                random(0, 2) ? 1 : 3;
            screensaver_pipe_direction =
                static_cast<int8_t>(
                    (screensaver_pipe_direction +
                     turn) %
                    4);
            screensaver_pipe_color++;
        }
        else
        {
            screensaver_pipe_x = next_x;
            screensaver_pipe_y = next_y;
        }
        return;
    }

    if (screensaver_active_mode ==
        ScreensaverMode::FlyingClocks)
    {
        const int16_t max_x =
            kScreensaverWidth - 82;
        const int16_t max_y =
            kScreensaverHeight - 28;
        for (size_t i = 0;
             i < kScreensaverFlyingClockCount;
             ++i)
        {
            int16_t x =
                screensaver_flying_x[i] +
                screensaver_flying_dx[i];
            int16_t y =
                screensaver_flying_y[i] +
                screensaver_flying_dy[i];
            if (x <= 0 || x >= max_x)
            {
                screensaver_flying_dx[i] =
                    -screensaver_flying_dx[i];
                x = constrain(x, 0, max_x);
            }
            if (y <= 0 || y >= max_y)
            {
                screensaver_flying_dy[i] =
                    -screensaver_flying_dy[i];
                y = constrain(y, 0, max_y);
            }
            screensaver_flying_x[i] = x;
            screensaver_flying_y[i] = y;
            lv_obj_set_pos(
                screensaver_flying_clocks[i],
                x, y);
        }
    }
}

#endif
