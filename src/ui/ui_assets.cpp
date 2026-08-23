#ifdef MACLOCK_COMBINED_SOURCE
void UiShell::init()
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    ui_shell.disk_missing_1_buf = load_png_once("S:/disk_missing_1.png");
    ui_shell.disk_missing_2_buf = load_png_once("S:/disk_missing_2.png");
    ui_shell.boot_buf = load_png_once("S:/boot.png");
    ui_shell.plugin_buf = load_png_once("S:/plugin.png");
    ui_shell.plugin_missing_buf = make_plugin_missing_buf(ui_shell.plugin_buf);

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

    ui_shell.boot_progress = lv_bar_create(ui_shell.boot);
    lv_obj_remove_flag(
        ui_shell.boot_progress, LV_OBJ_FLAG_SCROLLABLE);
    lv_bar_set_range(ui_shell.boot_progress, 0, 100);
    lv_bar_set_value(ui_shell.boot_progress, 0, LV_ANIM_OFF);
    lv_obj_set_size(ui_shell.boot_progress, 176, 10);
    lv_obj_set_pos(ui_shell.boot_progress, 74, 56);
    lv_obj_set_style_radius(ui_shell.boot_progress, 0, 0);
    lv_obj_set_style_border_width(ui_shell.boot_progress, 1, 0);
    lv_obj_set_style_border_color(
        ui_shell.boot_progress, lv_color_black(), 0);
    lv_obj_set_style_bg_color(
        ui_shell.boot_progress, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        ui_shell.boot_progress, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(
        ui_shell.boot_progress, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(
        ui_shell.boot_progress, lv_color_black(),
        LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(
        ui_shell.boot_progress, LV_OPA_COVER,
        LV_PART_INDICATOR);
    lv_obj_add_flag(
        ui_shell.boot_progress, LV_OBJ_FLAG_HIDDEN);

    for (size_t i = 0; i < k_plugin_max; ++i)
    {
        ui_shell.plugin_icons[i] = lv_image_create(scr);
        set_image_src(ui_shell.plugin_icons[i], ui_shell.plugin_buf, "S:/plugin.png");
    }

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
    maclock_hal().emulatorModeChanged(true);
    audio_service.stop();
    control_panel_service.stop();
    mqtt_service.stop(false);
    wifi_service.pause();
    audio_service.suspendTask();
    input_service.suspendTask();
    minivmac();

    maclock_hal().emulatorModeChanged(false);

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
             tr("Touchscreen"),
             snapshot.touchscreen_present
                 ? tr("Present")
                 : tr("Missing"),
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
