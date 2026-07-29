#ifdef MACLOCK_COMBINED_SOURCE
void UiShell::init()
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    ui_shell.background = lv_image_create(scr);
    ui_shell.background_buf = load_png_once("S:/background.png");
    ui_shell.corners_buf = load_png_once("S:/corners.png");
    ui_shell.disk_missing_1_buf = load_png_once("S:/disk_missing_1.png");
    ui_shell.disk_missing_2_buf = load_png_once("S:/disk_missing_2.png");
    ui_shell.boot_buf = load_png_once("S:/boot.png");
    ui_shell.menu_buf = load_png_once("S:/menu.png");
    ui_shell.menu_right_buf = load_png_once("S:/menu_right.png");
    ui_shell.icon_buf = load_png_once("S:/icon.png");
    ui_shell.clock_buf = load_png_once("S:/empty.png");
    ui_shell.alarm_indicator_buf =
        load_png_once("S:/alarm_indicator.png");
    ui_shell.plugin_buf = load_png_once("S:/plugin.png");
    ui_shell.plugin_missing_buf = make_plugin_missing_buf(ui_shell.plugin_buf);

    set_image_src(ui_shell.background, ui_shell.background_buf, "S:/background.png");
    lv_obj_center(ui_shell.background);

    ui_shell.white_bar = lv_obj_create(scr);
    lv_obj_remove_flag(
        ui_shell.white_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style_all(ui_shell.white_bar);
    lv_obj_set_size(ui_shell.white_bar, lv_pct(100), 19);
    lv_obj_set_pos(ui_shell.white_bar, 0, 0);
    lv_obj_set_style_bg_color(ui_shell.white_bar, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(ui_shell.white_bar, LV_OPA_COVER, 0);

    ui_shell.black_line = lv_obj_create(scr);
    lv_obj_remove_flag(
        ui_shell.black_line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style_all(ui_shell.black_line);
    lv_obj_set_size(ui_shell.black_line, lv_pct(100), 1);
    lv_obj_set_pos(ui_shell.black_line, 0, 19);
    lv_obj_set_style_bg_color(ui_shell.black_line, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ui_shell.black_line, LV_OPA_COVER, 0);

    ui_shell.menu = lv_image_create(scr);
    set_image_src(ui_shell.menu, ui_shell.menu_buf, "S:/menu.png");

    ui_shell.menu_titles = lv_label_create(scr);
    lv_obj_remove_style_all(ui_shell.menu_titles);
    lv_obj_set_size(ui_shell.menu_titles, 251, 19);
    lv_obj_set_pos(ui_shell.menu_titles, 37, 0);
    lv_obj_set_style_bg_color(
        ui_shell.menu_titles, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        ui_shell.menu_titles, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(
        ui_shell.menu_titles, lv_color_black(), 0);
    lv_obj_set_style_text_font(
        ui_shell.menu_titles, &lv_font_chicago_8, 0);
    lv_obj_set_style_pad_left(ui_shell.menu_titles, 4, 0);
    lv_obj_set_style_pad_top(ui_shell.menu_titles, 2, 0);
    lv_label_set_long_mode(
        ui_shell.menu_titles, LV_LABEL_LONG_CLIP);
    ui_shell.updateMenuTitles();

    ui_shell.menu_right = lv_image_create(scr);
    set_image_src(ui_shell.menu_right, ui_shell.menu_right_buf, "S:/menu_right.png");
    lv_obj_align(ui_shell.menu_right, LV_ALIGN_TOP_RIGHT, 0, 0);

    ui_shell.icon = lv_image_create(scr);
    set_image_src(ui_shell.icon, ui_shell.icon_buf, "S:/icon.png");
    lv_obj_align(ui_shell.icon, LV_ALIGN_TOP_RIGHT, -10, 30);

    ui_shell.clock = lv_image_create(scr);
    set_image_src(ui_shell.clock, ui_shell.clock_buf, "S:/clock.png");
    lv_obj_center(ui_shell.clock);

    ui_shell.clock_label = lv_label_create(ui_shell.clock);
    lv_label_set_text(ui_shell.clock_label, tr("Clock"));
    lv_obj_set_style_text_font(ui_shell.clock_label, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_letter_space(ui_shell.clock_label, 1, 0);
    lv_obj_set_style_bg_color(ui_shell.clock_label, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(ui_shell.clock_label, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(ui_shell.clock_label, 12, 0);
    lv_obj_set_style_pad_right(ui_shell.clock_label, 12, 0);
    lv_obj_align(ui_shell.clock_label, LV_ALIGN_TOP_MID, 0, 2);

    ui_shell.time = lv_label_create(ui_shell.clock);
    lv_label_set_text(ui_shell.time, "00:00:00");
    lv_obj_set_style_text_font(ui_shell.time, &lv_font_chicago_48, 0);
    lv_obj_set_style_text_letter_space(ui_shell.time, 1, 0);
    lv_obj_align(ui_shell.time, LV_ALIGN_TOP_MID, 0, 8);

    ui_shell.time_meridiem = lv_label_create(ui_shell.clock);
    lv_label_set_text(ui_shell.time_meridiem, "AM");
    lv_obj_set_style_text_font(
        ui_shell.time_meridiem, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_color(
        ui_shell.time_meridiem, lv_color_black(), 0);
    lv_obj_set_style_text_align(
        ui_shell.time_meridiem, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(
        ui_shell.time_meridiem, -2, 0);
    lv_obj_add_flag(
        ui_shell.time_meridiem, LV_OBJ_FLAG_HIDDEN);

    ui_shell.date = lv_label_create(ui_shell.clock);
    lv_label_set_text(
        ui_shell.date,
        g_date_format == UI_DATE_FORMAT_YMD
            ? "0000-00-00"
            : "00/00/0000");
    lv_obj_set_style_text_font(ui_shell.date, &lv_font_chicago_32, 0);
    lv_obj_set_style_text_letter_space(ui_shell.date, 1, 0);
    lv_obj_align(ui_shell.date, LV_ALIGN_TOP_MID, 0, 83);

    ui_shell.temp = lv_label_create(ui_shell.clock);
    char temperature_placeholder[12];
    snprintf(
        temperature_placeholder,
        sizeof(temperature_placeholder),
        "--.-°%c", display_temperature_unit());
    lv_label_set_text(ui_shell.temp, temperature_placeholder);
    lv_obj_set_style_text_font(ui_shell.temp, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_letter_space(ui_shell.temp, 1, 0);
    lv_obj_set_width(ui_shell.temp, 220);
    lv_obj_align(ui_shell.temp, LV_ALIGN_TOP_LEFT, 12, 118);

    ui_shell.gauge_icon = lv_image_create(ui_shell.clock);
    lv_image_set_src(ui_shell.gauge_icon, "S:/cloudy.png");
    lv_obj_align(ui_shell.gauge_icon, LV_ALIGN_TOP_RIGHT, -12, 111);

    ui_shell.gauge_line = lv_obj_create(ui_shell.clock);
    lv_obj_remove_flag(
        ui_shell.gauge_line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style_all(ui_shell.gauge_line);
    lv_obj_set_size(ui_shell.gauge_line, 180, 2);
    lv_obj_set_style_bg_color(ui_shell.gauge_line, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ui_shell.gauge_line, LV_OPA_COVER, 0);
    lv_obj_align(ui_shell.gauge_line, LV_ALIGN_TOP_RIGHT, -12, 127);

    ui_shell.gauge_box = lv_obj_create(ui_shell.clock);
    lv_obj_remove_flag(
        ui_shell.gauge_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style_all(ui_shell.gauge_box);
    lv_obj_set_size(ui_shell.gauge_box, 10, 10);
    lv_obj_set_style_border_color(ui_shell.gauge_box, lv_color_black(), 0);
    lv_obj_set_style_border_width(ui_shell.gauge_box, 1, 0);
    lv_obj_set_style_bg_color(ui_shell.gauge_box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(ui_shell.gauge_box, LV_OPA_COVER, 0);
    lv_obj_align(ui_shell.gauge_box, LV_ALIGN_TOP_RIGHT, -12, 124);

    ui_shell.alarm_indicator = lv_image_create(ui_shell.clock);
    set_image_src(ui_shell.alarm_indicator,
                  ui_shell.alarm_indicator_buf,
                  "S:/alarm_indicator.png");
    lv_obj_add_flag(
        ui_shell.alarm_indicator, LV_OBJ_FLAG_HIDDEN);

    ui_shell.disk_missing_1 = lv_image_create(scr);
    set_image_src(ui_shell.disk_missing_1, ui_shell.disk_missing_1_buf, "S:/disk_missing_1.png");
    lv_obj_center(ui_shell.disk_missing_1);

    ui_shell.disk_missing_2 = lv_image_create(scr);
    set_image_src(ui_shell.disk_missing_2, ui_shell.disk_missing_2_buf, "S:/disk_missing_2.png");
    lv_obj_center(ui_shell.disk_missing_2);

    ui_shell.boot = lv_image_create(scr);
    set_image_src(ui_shell.boot, ui_shell.boot_buf, "S:/boot.png");
    lv_obj_center(ui_shell.boot);

    ui_shell.boot_message = lv_label_create(ui_shell.boot);
    lv_obj_remove_style_all(ui_shell.boot_message);
    lv_obj_set_size(ui_shell.boot_message, 188, 24);
    lv_obj_set_pos(ui_shell.boot_message, 68, 25);
    lv_obj_set_style_bg_opa(
        ui_shell.boot_message, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(
        ui_shell.boot_message, lv_color_white(), 0);
    lv_obj_set_style_text_color(
        ui_shell.boot_message, lv_color_black(), 0);
    lv_obj_set_style_text_font(
        ui_shell.boot_message, &lv_font_chicago_8, 0);
    lv_obj_set_style_pad_left(ui_shell.boot_message, 6, 0);
    lv_obj_set_style_pad_top(ui_shell.boot_message, 2, 0);
    lv_label_set_long_mode(
        ui_shell.boot_message, LV_LABEL_LONG_CLIP);
    ui_shell.updateBootMessage();

    for (size_t i = 0; i < k_plugin_max; ++i)
    {
        ui_shell.plugin_icons[i] = lv_image_create(scr);
        set_image_src(ui_shell.plugin_icons[i], ui_shell.plugin_buf, "S:/plugin.png");
    }

    ui_shell.corners = lv_image_create(scr);
    set_image_src(ui_shell.corners, ui_shell.corners_buf, "S:/corners.png");
    lv_obj_center(ui_shell.corners);

    clock_view.init(scr);
    datetime_editor.begin(scr, app_events);
    alarm_view.begin(scr, app_events);
    timer_view.begin(scr, app_events);

    calibration_view.label = lv_label_create(scr);
    lv_label_set_text(calibration_view.label, tr("Touch the crosshair"));
    lv_obj_set_style_text_font(calibration_view.label, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_letter_space(calibration_view.label, 1, 0);
    lv_obj_align(calibration_view.label, LV_ALIGN_TOP_MID, 0, 24);

    calibration_view.cross = lv_label_create(scr);
    lv_label_set_text(calibration_view.cross, "+");
    lv_obj_set_style_text_font(calibration_view.cross, &lv_font_chicago_32, 0);

    g_cursor = lv_image_create(scr);
    lv_image_set_src(g_cursor, "S:/cursor.png");
    lv_obj_clear_flag(g_cursor, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_cursor, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(scr, screen_touch_event, LV_EVENT_PRESSED, NULL);

    boot_options_view.init(scr);
    diagnostics_view.init(scr);
    wifi_setup_view.init(scr);
    mqtt_notification_view.init(scr);

    ui_shell.hideAll();
}

static void run_emulator()
{
    audio_service.stop();
    control_panel_service.stop();
    mqtt_service.stop(false);
    wifi_service.pause();
    audio_service.suspendTask();
    input_service.suspendTask();

    minivmac();

    input_service.resumeTask();
    audio_service.resumeTask();
    wifi_service.resume();
}

void DiagnosticsView::update(
    const DiagnosticsSnapshot &snapshot)
{
    const WifiModeSnapshot &wifi = snapshot.wifi;
    const char *network_state =
        !wifi.enabled
            ? tr("Disabled")
            : (wifi.portal_active
                   ? tr("Setup portal")
                   : (!wifi.configured
                          ? tr("Not configured")
                          : (wifi.connected
                                 ? tr("Online")
                                 : tr("Offline"))));
    const char *network_ssid =
        wifi.ssid[0] ? wifi.ssid : "--";
    char network_address[40];
    if (wifi.connected && wifi.ip_address[0])
    {
        snprintf(network_address, sizeof(network_address),
                 "%s / %ld dBm",
                 wifi.ip_address, (long)wifi.rssi);
    }
    else
    {
        snprintf(network_address, sizeof(network_address), "--");
    }

    char status[480];
    snprintf(status, sizeof(status),
             "%s: %s\n"
             "%s: %s\n"
             "%s: %s\n"
             "%s: %lld/%u\n"
             "%s: %s\n"
             "%s: %s\n"
             "%-7s: %s\n"
             "%-7s: %s\n"
             "%-7s: %s\n"
             "%-7s: %s\n"
             "%s",
             tr("Clock"),
             snapshot.clock_pressed
                 ? tr("Pressed")
                 : tr("Released"),
             tr("Alarm"),
             snapshot.alarm_pressed
                 ? tr("Pressed")
                 : tr("Released"),
             tr("Floppy"),
             snapshot.floppy_inserted
                 ? tr("Inserted")
                 : tr("Empty"),
             tr("Encoder"),
             (long long)snapshot.encoder_position,
             (unsigned)kBrightnessMax,
             tr("Touch"),
             snapshot.touch_pressed
                 ? tr("Pressed")
                 : tr("Released"),
             tr("Charging"),
             snapshot.charging ? tr("Yes") : tr("No"),
             tr("I2C"),
             snapshot.i2c_devices[0]
                 ? snapshot.i2c_devices
                 : tr("None"),
             tr("Wi-Fi"),
             network_state,
             tr("SSID"),
             network_ssid,
             tr("IP/RSSI"),
             network_address,
             snapshot.rtc_status);
    lv_label_set_text(diagnostics_view.status, status);
}

#endif
