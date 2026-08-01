#ifdef MACLOCK_COMBINED_SOURCE
void DiagnosticsView::init(lv_obj_t *screen)
{
    diagnostics_view.panel = lv_obj_create(screen);
    lv_obj_remove_flag(
        diagnostics_view.panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(diagnostics_view.panel, 286, 230);
    lv_obj_center(diagnostics_view.panel);
    lv_obj_set_style_bg_color(diagnostics_view.panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(diagnostics_view.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(diagnostics_view.panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(diagnostics_view.panel, 2, 0);
    lv_obj_set_style_radius(diagnostics_view.panel, 0, 0);
    lv_obj_set_style_pad_all(diagnostics_view.panel, 6, 0);

    diagnostics_view.title = lv_label_create(diagnostics_view.panel);
    lv_label_set_text(diagnostics_view.title, tr("Hardware Diagnostics"));
    lv_obj_set_style_text_font(diagnostics_view.title, &lv_font_chicago_8, 0);
    lv_obj_align(diagnostics_view.title, LV_ALIGN_TOP_MID, 0, 0);

    diagnostics_view.status = lv_label_create(diagnostics_view.panel);
    lv_label_set_text(diagnostics_view.status, tr("Checking hardware..."));
    lv_obj_set_width(diagnostics_view.status, lv_pct(100));
    lv_obj_set_style_text_font(diagnostics_view.status,
                               &lv_font_chicago_8, 0);
    lv_obj_set_style_text_line_space(diagnostics_view.status, 0, 0);
    lv_obj_align(diagnostics_view.status, LV_ALIGN_TOP_LEFT, 0, 18);

    lv_obj_t *back_button =
        create_action_button(diagnostics_view.panel, tr("Back"),
                             diagnostics_back_event);
    diagnostics_view.back_label = lv_obj_get_child(back_button, 0);
    lv_obj_set_size(back_button, 80, 24);
    lv_obj_align(back_button, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_add_flag(diagnostics_view.panel, LV_OBJ_FLAG_HIDDEN);
}

static void update_wifi_setup_status_label(
    const char *status)
{
    if (!wifi_setup_view.status)
        return;

    char setup_status[180];
    snprintf(
        setup_status, sizeof(setup_status),
        tr("Scan to join\nMaclock Setup\n"
           "(iPhone / Android)\n\n"
           "Open 192.168.4.1\n%s"),
        tr(status));
    lv_label_set_text(
        wifi_setup_view.status, setup_status);
}

void WifiSetupView::init(lv_obj_t *screen)
{
    wifi_setup_view.panel = lv_obj_create(screen);
    lv_obj_remove_flag(
        wifi_setup_view.panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(wifi_setup_view.panel, 286, 208);
    lv_obj_center(wifi_setup_view.panel);
    lv_obj_set_style_bg_color(
        wifi_setup_view.panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        wifi_setup_view.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        wifi_setup_view.panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(wifi_setup_view.panel, 2, 0);
    lv_obj_set_style_radius(wifi_setup_view.panel, 0, 0);
    lv_obj_set_style_pad_all(wifi_setup_view.panel, 8, 0);

    wifi_setup_view.title = lv_label_create(wifi_setup_view.panel);
    lv_label_set_text(wifi_setup_view.title, tr("Wi-Fi Setup"));
    lv_obj_set_style_text_font(wifi_setup_view.title, &lv_font_chicago_8, 0);
    lv_obj_align(wifi_setup_view.title, LV_ALIGN_TOP_MID, 0, 0);

    wifi_setup_view.qr =
        lv_qrcode_create(wifi_setup_view.panel);
    lv_qrcode_set_size(wifi_setup_view.qr, 112);
    lv_qrcode_set_dark_color(
        wifi_setup_view.qr, lv_color_black());
    lv_qrcode_set_light_color(
        wifi_setup_view.qr, lv_color_white());
    lv_qrcode_set_quiet_zone(
        wifi_setup_view.qr, true);
    // Standard Wi-Fi QR payload recognized by both iOS and Android.
    // The setup access point is intentionally open.
    lv_qrcode_set_data(
        wifi_setup_view.qr,
        "WIFI:T:nopass;S:Maclock Setup;;");
    lv_obj_align(
        wifi_setup_view.qr,
        LV_ALIGN_TOP_LEFT, 0, 24);

    wifi_setup_view.status =
        lv_label_create(wifi_setup_view.panel);
    update_wifi_setup_status_label(
        "Connect to Maclock Setup");
    lv_obj_set_width(wifi_setup_view.status, 140);
    lv_obj_set_style_text_font(
        wifi_setup_view.status, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_align(
        wifi_setup_view.status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(
        wifi_setup_view.status, 2, 0);
    lv_obj_align(
        wifi_setup_view.status, LV_ALIGN_TOP_RIGHT, 0, 28);

    lv_obj_t *back_button =
        create_action_button(
            wifi_setup_view.panel, tr("Back"),
            wifi_setup_back_event);
    wifi_setup_view.back_label = lv_obj_get_child(back_button, 0);
    lv_obj_set_size(back_button, 100, 38);
    lv_obj_align(back_button, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_add_flag(
        wifi_setup_view.panel, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_language_ui()
{
    update_boot_translation_maps();
    if (!boot_options_view.panel)
        return;

    lv_buttonmatrix_set_map(
        boot_options_view.brightness_options, g_brightness_map);
    lv_buttonmatrix_set_map(
        boot_options_view.clock_face_options, g_clock_face_map);
    lv_buttonmatrix_set_map(
        boot_options_view.face_accent_options,
        g_face_accent_map);
    lv_buttonmatrix_set_map(
        boot_options_view.face_size_options,
        g_face_size_map);
    lv_buttonmatrix_set_map(
        boot_options_view.flip_speed_options,
        g_flip_speed_map);
    lv_buttonmatrix_set_map(
        boot_options_view.screensaver_delay_options,
        g_screensaver_delay_map);
    lv_buttonmatrix_set_map(
        boot_options_view.night_enabled_options, g_night_enabled_map);
    lv_buttonmatrix_set_map(
        boot_options_view.night_screen_options, g_night_screen_map);
    lv_buttonmatrix_set_map(
        boot_options_view.chime_mode_options, g_chime_mode_map);
    lv_buttonmatrix_set_map(
        boot_options_view.chime_quiet_options, g_chime_quiet_map);
    lv_buttonmatrix_set_map(
        boot_options_view.wifi_enabled_options, g_wifi_enabled_map);

    lv_label_set_text(boot_options_view.brightness_label, tr("Brightness"));
    lv_label_set_text(
        boot_options_view.screensaver_delay_label,
        tr("Start after"));
    lv_label_set_text(
        boot_options_view.face_accent_label,
        tr("Accent"));
    lv_label_set_text(
        boot_options_view.face_size_label,
        tr("Numeral size"));
    lv_label_set_text(
        boot_options_view.flip_speed_label,
        tr("Flip speed"));
    lv_label_set_text(boot_options_view.dim_from_label, tr("Dim from"));
    lv_label_set_text(boot_options_view.normal_at_label, tr("Normal at"));
    lv_label_set_text(boot_options_view.screen_off_label, tr("Screen off at"));
    lv_label_set_text(boot_options_view.quiet_from_label, tr("Quiet from"));
    lv_label_set_text(boot_options_view.quiet_end_label, tr("Quiet ends"));
    lv_label_set_text(boot_options_view.wifi_setup_label, tr("Setup Wi-Fi"));
    lv_label_set_text(boot_options_view.clock_button_label, tr("Clock"));
    lv_label_set_text(boot_options_view.emulator_button_label, tr("Emulator"));
    update_boot_mode_button();
    lv_label_set_text(boot_options_view.diagnostics_button_label, tr("Diagnostics"));
    lv_label_set_text(
        boot_options_view.section_labels[BOOT_OPTIONS_SECTION_GENERAL],
        tr("General"));
    lv_label_set_text(
        boot_options_view.section_labels[BOOT_OPTIONS_SECTION_DISPLAY],
        tr("Display"));
    lv_label_set_text(
        boot_options_view.section_labels[BOOT_OPTIONS_SECTION_SOUND],
        tr("Sound"));
    lv_label_set_text(
        boot_options_view.section_labels[BOOT_OPTIONS_SECTION_SYSTEM],
        tr("System"));
    lv_label_set_text(
        boot_options_view.about_author,
        tr("Author: fensoft"));
    lv_label_set_text(
        boot_options_view.calibration_label,
        tr("Press Clock for screen calibration"));
    lv_label_set_text(
        boot_options_view.home_calibration_label,
        tr("Click again for calibration"));
    lv_label_set_text(boot_options_view.previous_label, tr("Previous"));
    lv_label_set_text(boot_options_view.next_label, tr("Next"));
    boot_options_view.chime_sound_selector.refreshLanguage();
    boot_options_view.refreshDateTime();

    lv_label_set_text(diagnostics_view.title, tr("Hardware Diagnostics"));
    lv_label_set_text(diagnostics_view.back_label, tr("Back"));
    lv_label_set_text(wifi_setup_view.title, tr("Wi-Fi Setup"));
    lv_label_set_text(wifi_setup_view.back_label, tr("Back"));
    update_wifi_setup_status_label(
        wifi_service.snapshot().status);
    ui_shell.updateBootMessage();
    ui_shell.updateMenuTitles();
    lv_label_set_text(ui_shell.clock_label, tr("Clock"));
    lv_label_set_text(
        clock_view.compact_title, tr("Clock"));
    lv_label_set_text(
        clock_view.flip_title, tr("Clock"));
    lv_label_set_text(calibration_view.label, tr("Touch the crosshair"));

    alarm_view.refreshLanguage();
    timer_view.refreshLanguage();
    datetime_editor.refreshLanguage();
    update_language_selection(true);
    set_checked_button(
        boot_options_view.brightness_options,
        (uint32_t)g_boot_brightness);
    update_regional_options_ui();
    update_display_options_ui();
    update_face_customization_options_ui();
    set_checked_button(
        boot_options_view.clock_face_options,
        (uint32_t)g_clock_face);
    update_screensaver_mode_button();
    set_checked_button(
        boot_options_view.screensaver_delay_options,
        g_screensaver_delay_index);
    update_night_options_ui();
    update_chime_options_ui();
    update_wifi_options_ui();
    boot_options_view.setPage(boot_options_view.page);
    diagnostics_view.update(make_diagnostics_snapshot());
}

void UiShell::hideAll()
{
    lv_obj_add_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.disk_missing_1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.disk_missing_2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.boot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.menu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.menu_titles, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.menu_right, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.clock, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.clock_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.time, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.date, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.temp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.gauge_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.gauge_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.gauge_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.alarm_indicator, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.white_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_shell.black_line, LV_OBJ_FLAG_HIDDEN);
    if (clock_view.compact)
        lv_obj_add_flag(
            clock_view.compact, LV_OBJ_FLAG_HIDDEN);
    if (clock_view.analog)
        lv_obj_add_flag(
            clock_view.analog, LV_OBJ_FLAG_HIDDEN);
    if (clock_view.flip)
        lv_obj_add_flag(
            clock_view.flip, LV_OBJ_FLAG_HIDDEN);
    if (clock_view.odometer)
        lv_obj_add_flag(
            clock_view.odometer, LV_OBJ_FLAG_HIDDEN);
    if (clock_view.screensaver)
        lv_obj_add_flag(
            clock_view.screensaver, LV_OBJ_FLAG_HIDDEN);
    if (boot_options_view.panel)
        lv_obj_add_flag(boot_options_view.panel, LV_OBJ_FLAG_HIDDEN);
    if (diagnostics_view.panel)
        lv_obj_add_flag(diagnostics_view.panel, LV_OBJ_FLAG_HIDDEN);
    if (wifi_setup_view.panel)
        lv_obj_add_flag(wifi_setup_view.panel, LV_OBJ_FLAG_HIDDEN);
    for (size_t i = 0; i < k_plugin_max; ++i)
    {
        if (ui_shell.plugin_icons[i])
            lv_obj_add_flag(ui_shell.plugin_icons[i], LV_OBJ_FLAG_HIDDEN);
    }
    datetime_editor.hide();
    alarm_view.hide();
    timer_view.hide();
    if (calibration_view.label)
        lv_obj_add_flag(calibration_view.label, LV_OBJ_FLAG_HIDDEN);
    if (calibration_view.cross)
        lv_obj_add_flag(calibration_view.cross, LV_OBJ_FLAG_HIDDEN);
}

static lv_draw_buf_t *load_png_once(const char *path)
{
    lv_image_decoder_dsc_t dsc;
    if (lv_image_decoder_open(&dsc, path, NULL) != LV_RESULT_OK)
        return NULL;

    lv_draw_buf_t *dup = lv_draw_buf_dup(dsc.decoded);
    lv_image_decoder_close(&dsc);
    return dup;
}

static void replace_black_with_red(lv_draw_buf_t *buf)
{
    if (!buf)
        return;

    const lv_color_format_t cf = (lv_color_format_t)buf->header.cf;
    const uint32_t w = buf->header.w;
    const uint32_t h = buf->header.h;

    if (LV_COLOR_FORMAT_IS_INDEXED(cf))
    {
        const uint32_t palette_size = LV_COLOR_INDEXED_PALETTE_SIZE(cf);
        lv_color32_t *palette = (lv_color32_t *)buf->data;
        for (uint32_t i = 0; i < palette_size; ++i)
        {
            const lv_color32_t c = palette[i];
            if (c.red <= 8 && c.green <= 8 && c.blue <= 8 && c.alpha > 0)
                lv_draw_buf_set_palette(buf, (uint8_t)i, lv_color32_make(255, 0, 0, c.alpha));
        }
        return;
    }

    if (cf == LV_COLOR_FORMAT_RGB565)
    {
        const uint16_t red_565 = (uint16_t)0xF800;
        uint8_t *row = buf->data;
        const uint32_t stride = buf->header.stride;
        for (uint32_t y = 0; y < h; ++y)
        {
            uint16_t *px = (uint16_t *)row;
            for (uint32_t x = 0; x < w; ++x)
            {
                if (px[x] == 0x0000)
                    px[x] = red_565;
            }
            row += stride;
        }
        return;
    }

    if (cf == LV_COLOR_FORMAT_ARGB8888 || cf == LV_COLOR_FORMAT_XRGB8888 || cf == LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED)
    {
        uint8_t *row = buf->data;
        const uint32_t stride = buf->header.stride;
        for (uint32_t y = 0; y < h; ++y)
        {
            uint8_t *px = row;
            for (uint32_t x = 0; x < w; ++x)
            {
                uint8_t *b = &px[x * 4 + 0];
                uint8_t *g = &px[x * 4 + 1];
                uint8_t *r = &px[x * 4 + 2];
                uint8_t *a = &px[x * 4 + 3];
                if (*r <= 8 && *g <= 8 && *b <= 8 && *a > 0)
                {
                    *b = 0;
                    *g = 0;
                    *r = (cf == LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED) ? *a : 255;
                }
            }
            row += stride;
        }
    }
}

static lv_draw_buf_t *make_plugin_missing_buf(lv_draw_buf_t *src)
{
    if (!src)
        return NULL;
    lv_draw_buf_t *dup = lv_draw_buf_dup(src);
    if (!dup)
        return NULL;
    replace_black_with_red(dup);
    return dup;
}

static void set_image_src(lv_obj_t *img, lv_draw_buf_t *buf, const char *path)
{
    if (buf)
        lv_image_set_src(img, (const lv_image_dsc_t *)buf);
    else
        lv_image_set_src(img, path);
}

void UiShell::releaseMacOS8Assets()
{
    if (ui_shell.background)
    {
        set_image_src(
            ui_shell.background, ui_shell.background_buf,
            "S:/background.png");
        set_image_src(
            ui_shell.corners, ui_shell.corners_buf,
            "S:/corners.png");
        set_image_src(
            ui_shell.menu, ui_shell.menu_buf, "S:/menu.png");
        set_image_src(
            ui_shell.menu_right, ui_shell.menu_right_buf,
            "S:/menu_right.png");
        set_image_src(
            ui_shell.icon, ui_shell.icon_buf, "S:/icon.png");
        set_image_src(
            ui_shell.clock, ui_shell.clock_buf, "S:/empty.png");
        set_image_src(
            ui_shell.alarm_indicator,
            ui_shell.alarm_indicator_buf,
            "S:/alarm_indicator.png");
        lv_image_set_src(
            ui_shell.gauge_icon, "S:/cloudy.png");
        lv_obj_align(
            ui_shell.icon, LV_ALIGN_TOP_RIGHT, -10, 30);
        lv_obj_center(ui_shell.clock);
    }

    lv_draw_buf_t **buffers[] = {
        &ui_shell.macos8_background_buf,
        &ui_shell.macos8_corners_buf,
        &ui_shell.macos8_menu_buf,
        &ui_shell.macos8_floppy_buf,
        &ui_shell.macos8_clock_buf,
        &ui_shell.macos8_alarm_buf,
    };
    for (lv_draw_buf_t **buffer : buffers)
    {
        if (*buffer)
        {
            lv_draw_buf_destroy(*buffer);
            *buffer = nullptr;
        }
    }
    ui_shell.macos8_assets_loaded = false;
}

void UiShell::applyMacintoshAppearance(bool macos8)
{
    if (!macos8)
    {
        ui_shell.releaseMacOS8Assets();
        lv_obj_set_style_bg_color(
            ui_shell.white_bar, lv_color_white(), 0);
        lv_obj_set_style_bg_color(
            ui_shell.black_line, lv_color_black(), 0);
        lv_obj_set_style_bg_color(
            ui_shell.menu_titles, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(
            ui_shell.menu_titles, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(
            ui_shell.menu_titles, lv_color_black(), 0);
        lv_obj_set_style_bg_color(
            ui_shell.clock_label, lv_color_white(), 0);
        lv_obj_set_style_text_color(
            ui_shell.clock_label, lv_color_black(), 0);
        for (lv_obj_t *label : {
                 ui_shell.time, ui_shell.time_meridiem,
                 ui_shell.date, ui_shell.temp})
        {
            lv_obj_set_style_text_color(label, lv_color_black(), 0);
        }
        lv_obj_set_style_bg_color(
            ui_shell.gauge_line, lv_color_black(), 0);
        lv_obj_set_style_border_color(
            ui_shell.gauge_box, lv_color_black(), 0);
        lv_obj_set_style_bg_color(
            ui_shell.gauge_box, lv_color_white(), 0);
        return;
    }

    if (!ui_shell.macos8_assets_loaded)
    {
        ui_shell.releaseMacOS8Assets();
        #define LOAD_MACOS8_ASSET(field, name)                   \
            ui_shell.field = load_png_once("S:/macos8_" name ".png")
        LOAD_MACOS8_ASSET(macos8_background_buf, "background");
        LOAD_MACOS8_ASSET(macos8_corners_buf, "corners");
        LOAD_MACOS8_ASSET(macos8_menu_buf, "menu");
        LOAD_MACOS8_ASSET(macos8_floppy_buf, "floppy");
        LOAD_MACOS8_ASSET(macos8_clock_buf, "empty");
        LOAD_MACOS8_ASSET(macos8_alarm_buf, "alarm");
#undef LOAD_MACOS8_ASSET

        ui_shell.macos8_assets_loaded =
            ui_shell.macos8_background_buf &&
            ui_shell.macos8_corners_buf &&
            ui_shell.macos8_menu_buf &&
            ui_shell.macos8_floppy_buf &&
            ui_shell.macos8_clock_buf &&
            ui_shell.macos8_alarm_buf;
        if (!ui_shell.macos8_assets_loaded)
        {
            Serial.println(
                "[UI] Mac OS 8 assets unavailable; "
                "using Macintosh appearance");
            ui_shell.applyMacintoshAppearance(false);
            return;
        }
    }

    set_image_src(
        ui_shell.background, ui_shell.macos8_background_buf,
        "S:/macos8_background.png");
    set_image_src(
        ui_shell.corners, ui_shell.macos8_corners_buf,
        "S:/macos8_corners.png");
    set_image_src(
        ui_shell.menu, ui_shell.macos8_menu_buf,
        "S:/macos8_menu.png");
    set_image_src(
        ui_shell.icon, ui_shell.macos8_floppy_buf,
        "S:/macos8_floppy.png");
    set_image_src(
        ui_shell.clock, ui_shell.macos8_clock_buf,
        "S:/macos8_empty.png");
    set_image_src(
        ui_shell.alarm_indicator, ui_shell.macos8_alarm_buf,
        "S:/macos8_alarm.png");
    lv_image_set_src(
        ui_shell.gauge_icon, "S:/macos8_cloudy.png");
    lv_obj_align(
        ui_shell.icon, LV_ALIGN_TOP_RIGHT, -10, 30);
    lv_obj_center(ui_shell.clock);

    const lv_color_t chrome = lv_color_hex(0xDDDDDD);
    const lv_color_t foreground = lv_color_black();
    const lv_color_t content = lv_color_white();
    const lv_color_t shadow = lv_color_hex(0x777777);
    lv_obj_set_style_bg_color(ui_shell.white_bar, chrome, 0);
    lv_obj_set_style_bg_color(ui_shell.black_line, shadow, 0);
    lv_obj_set_style_bg_color(ui_shell.menu_titles, chrome, 0);
    lv_obj_set_style_bg_opa(
        ui_shell.menu_titles, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(
        ui_shell.menu_titles, foreground, 0);
    lv_obj_set_style_bg_color(ui_shell.clock_label, chrome, 0);
    lv_obj_set_style_text_color(
        ui_shell.clock_label, foreground, 0);
    for (lv_obj_t *label : {
             ui_shell.time, ui_shell.time_meridiem,
             ui_shell.date, ui_shell.temp})
    {
        lv_obj_set_style_text_color(label, foreground, 0);
    }
    lv_obj_set_style_bg_color(
        ui_shell.gauge_line, foreground, 0);
    lv_obj_set_style_border_color(
        ui_shell.gauge_box, foreground, 0);
    lv_obj_set_style_bg_color(
        ui_shell.gauge_box, content, 0);
}

static bool littlefs_exists(const char *path)
{
    return LittleFS.exists(path);
}

static float display_temperature(float celsius)
{
    return g_temperature_unit == UI_TEMPERATURE_FAHRENHEIT
               ? celsius * 9.0f / 5.0f + 32.0f
               : celsius;
}

static char display_temperature_unit()
{
    return g_temperature_unit == UI_TEMPERATURE_FAHRENHEIT
               ? 'F'
               : 'C';
}

static void format_display_date(
    const DateTime &date, char *text, size_t text_size)
{
    char formatted_date[16];
    switch (g_date_format)
    {
    case UI_DATE_FORMAT_MDY:
        snprintf(
            formatted_date, sizeof(formatted_date),
            "%02d/%02d/%04d",
            date.month(), date.day(), date.year());
        break;
    case UI_DATE_FORMAT_YMD:
        snprintf(
            formatted_date, sizeof(formatted_date),
            "%04d-%02d-%02d",
            date.year(), date.month(), date.day());
        break;
    default:
        snprintf(
            formatted_date, sizeof(formatted_date),
            "%02d/%02d/%04d",
            date.day(), date.month(), date.year());
        break;
    }

    if (!g_time_format.show_weekday)
    {
        snprintf(text, text_size, "%s", formatted_date);
        return;
    }

    static const char *weekday_keys[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const uint8_t weekday = date.dayOfTheWeek();
    snprintf(
        text, text_size, "%s %s",
        tr(weekday_keys[weekday < 7 ? weekday : 0]),
        formatted_date);
}

#endif
