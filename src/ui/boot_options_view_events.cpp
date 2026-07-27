#ifdef MACLOCK_COMBINED_SOURCE
static void set_checked_button(lv_obj_t *matrix, uint32_t selected)
{
    if (!matrix)
        return;
    lv_buttonmatrix_clear_button_ctrl_all(
        matrix, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_button_ctrl(
        matrix, selected, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_selected_button(matrix, selected);
}

static void update_language_selection(bool scroll_to_selected)
{
    const uint32_t selected =
        (uint32_t)localization_get_language();
    for (uint32_t i = 0; i < UI_LANGUAGE_COUNT; ++i)
    {
        lv_obj_t *item = boot_options_view.language_items[i];
        if (!item)
            continue;
        if (i == selected)
            lv_obj_add_state(item, LV_STATE_CHECKED);
        else
            lv_obj_remove_state(item, LV_STATE_CHECKED);
    }

    if (scroll_to_selected &&
        selected < UI_LANGUAGE_COUNT &&
        boot_options_view.language_items[selected])
    {
        lv_obj_scroll_to_view(
            boot_options_view.language_items[selected],
            LV_ANIM_OFF);
    }
}

void UiShell::updateMenuTitles()
{
    if (!ui_shell.menu_titles)
        return;

    char titles[96];
    snprintf(
        titles, sizeof(titles), "%s  %s  %s  %s",
        tr("File"), tr("Edit"), tr("View"), tr("Special"));
    lv_label_set_text(ui_shell.menu_titles, titles);
}

void UiShell::updateBootMessage()
{
    if (ui_shell.boot_message)
        lv_label_set_text(
            ui_shell.boot_message, tr("Welcome to Macintosh."));
}

static void update_boot_translation_maps()
{
    g_brightness_map[0] = tr("Latest");
    g_brightness_map[1] = tr("Lowest");
    g_brightness_map[2] = tr("Highest");
    g_brightness_map[3] = "";
    g_remember_map[0] = tr("One time");
    g_remember_map[1] = tr("Remember");
    g_remember_map[2] = "";
    g_clock_face_map[0] = tr("Macintosh");
    g_clock_face_map[1] = tr("Compact");
    g_clock_face_map[2] = "\n";
    g_clock_face_map[3] = tr("Analog");
    g_clock_face_map[4] = tr("Flip");
    g_clock_face_map[5] = "";
    g_clock_theme_map[0] = tr("Light");
    g_clock_theme_map[1] = tr("Dark");
    g_clock_theme_map[2] = "";
    g_screensaver_map[0] = tr("Off");
    g_screensaver_map[1] = tr("After Dark");
    g_screensaver_map[2] = "";
    g_screensaver_delay_map[0] = tr("1 min");
    g_screensaver_delay_map[1] = tr("5 min");
    g_screensaver_delay_map[2] = "\n";
    g_screensaver_delay_map[3] = tr("10 min");
    g_screensaver_delay_map[4] = tr("30 min");
    g_screensaver_delay_map[5] = "";
    g_night_enabled_map[0] = tr("Disabled");
    g_night_enabled_map[1] = tr("Enabled");
    g_night_enabled_map[2] = "";
    g_night_screen_map[0] = tr("Dim only");
    g_night_screen_map[1] = tr("Screen off");
    g_night_screen_map[2] = "";
    g_chime_mode_map[0] = tr("Off");
    g_chime_mode_map[1] = "\n";
    g_chime_mode_map[2] = tr("Hourly");
    g_chime_mode_map[3] = "\n";
    g_chime_mode_map[4] = tr("Quarter hour");
    g_chime_mode_map[5] = "";
    g_chime_quiet_map[0] = tr("Disabled");
    g_chime_quiet_map[1] = tr("Enabled");
    g_chime_quiet_map[2] = "";
    g_wifi_enabled_map[0] = tr("Off");
    g_wifi_enabled_map[1] = tr("On");
    g_wifi_enabled_map[2] = "";
}

static void update_night_options_ui()
{
    snprintf(g_night_start_text, sizeof(g_night_start_text),
             "%02u:00", (unsigned)g_night_mode.start_hour);
    snprintf(g_night_end_text, sizeof(g_night_end_text),
             "%02u:00", (unsigned)g_night_mode.end_hour);
    snprintf(g_night_off_text, sizeof(g_night_off_text),
             "%02u:00", (unsigned)g_night_mode.screen_off_hour);

    if (boot_options_view.night_start_options)
    {
        lv_buttonmatrix_set_map(
            boot_options_view.night_start_options, g_night_start_map);
        lv_buttonmatrix_set_map(
            boot_options_view.night_end_options, g_night_end_map);
        lv_buttonmatrix_set_map(
            boot_options_view.night_off_options, g_night_off_map);
        set_checked_button(
            boot_options_view.night_enabled_options,
            g_night_mode.enabled ? 1 : 0);
        set_checked_button(
            boot_options_view.night_screen_options,
            g_night_mode.screen_off_enabled ? 1 : 0);
    }
}

static void update_chime_options_ui()
{
    snprintf(
        g_chime_quiet_start_text,
        sizeof(g_chime_quiet_start_text),
        "%02u:00", (unsigned)g_chime.quiet_start_hour);
    snprintf(
        g_chime_quiet_end_text,
        sizeof(g_chime_quiet_end_text),
        "%02u:00", (unsigned)g_chime.quiet_end_hour);

    if (!boot_options_view.chime_mode_options)
        return;
    lv_buttonmatrix_set_map(
        boot_options_view.chime_quiet_start_options,
        g_chime_quiet_start_map);
    lv_buttonmatrix_set_map(
        boot_options_view.chime_quiet_end_options,
        g_chime_quiet_end_map);
    set_checked_button(
        boot_options_view.chime_mode_options, (uint32_t)g_chime.mode);
    boot_options_view.chime_sound_selector.setPath(
        g_chime_sound_path);
    boot_options_view.chime_sound_selector.setPreviewVolume(
        g_chime_volumes[g_chime.volume]);
    set_checked_button(
        boot_options_view.chime_volume_options, g_chime.volume);
    set_checked_button(
        boot_options_view.chime_quiet_options,
        g_chime.quiet_enabled ? 1 : 0);
}

static void update_wifi_options_ui()
{
    if (!boot_options_view.wifi_enabled_options)
        return;

    const WifiModeSnapshot wifi = wifi_service.snapshot();
    set_checked_button(
        boot_options_view.wifi_enabled_options,
        wifi.enabled ? 1 : 0);

    char status[144];
    if (!wifi.enabled)
    {
        snprintf(status, sizeof(status), "%s",
                 tr("Wi-Fi disabled\nClock remains fully offline"));
    }
    else if (!wifi.configured)
    {
        snprintf(status, sizeof(status), "%s",
                 tr("Setup required\nChoose Setup Wi-Fi below"));
    }
    else if (wifi.connected)
    {
        snprintf(status, sizeof(status), tr("Online: %s\n%s"),
                 wifi.location[0] ? wifi.location : wifi.city,
                 wifi.timezone[0]
                     ? wifi.timezone
                     : tr(wifi.status));
    }
    else
    {
        snprintf(status, sizeof(status), "%s\n%s",
                 wifi.ssid, tr(wifi.status));
    }
    lv_label_set_text(boot_options_view.wifi_status, status);
}

static void language_event(lv_event_t *event)
{
    lv_obj_t *item = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        (uint32_t)(uintptr_t)lv_obj_get_user_data(item);
    if (selected >= UI_LANGUAGE_COUNT)
        return;
    app_settings.language = (UiLanguage)selected;
    localization_set_language(app_settings.language);
    settings_store.saveLanguage(app_settings.language);
    refresh_language_ui();
}

static void date_format_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= UI_DATE_FORMAT_COUNT)
        return;
    g_date_format = (UiDateFormat)selected;
    settings_store.saveDateFormat(g_date_format);
    datetime_editor.setDateFormat(g_date_format);
}

static void temperature_unit_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= UI_TEMPERATURE_UNIT_COUNT)
        return;
    g_temperature_unit = (UiTemperatureUnit)selected;
    settings_store.saveTemperatureUnit(g_temperature_unit);
}

static void clock_face_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= CLOCK_FACE_COUNT)
        return;
    g_clock_face = (ClockFace)selected;
    settings_store.saveClockFace(g_clock_face);
}

static void clock_theme_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= CLOCK_THEME_COUNT)
        return;
    g_clock_theme = (ClockTheme)selected;
    settings_store.saveClockTheme(g_clock_theme);
    if (clock_view.compact)
        clock_view.applyTheme();
}

static void screensaver_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= SCREENSAVER_MODE_COUNT)
        return;
    g_screensaver_mode = (ScreensaverMode)selected;
    settings_store.saveScreensaverMode(g_screensaver_mode);
}

static void screensaver_delay_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >=
        sizeof(g_screensaver_delays_minutes) /
            sizeof(g_screensaver_delays_minutes[0]))
    {
        return;
    }
    g_screensaver_delay_index = (uint8_t)selected;
    settings_store.saveScreensaverDelay(
        g_screensaver_delay_index);
}

static void chime_mode_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= CHIME_MODE_COUNT)
        return;
    g_chime.mode = (ChimeMode)selected;
    settings_store.saveChime(g_chime, g_chime_sound_path);
}

static void chime_sound_changed(
    const char *path, void *user_data)
{
    (void)user_data;
    if (!path)
        return;
    strlcpy(
        g_chime_sound_path, path,
        sizeof(g_chime_sound_path));
    settings_store.saveChime(g_chime, g_chime_sound_path);
}

static void chime_volume_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >=
        sizeof(g_chime_volumes) / sizeof(g_chime_volumes[0]))
    {
        return;
    }
    g_chime.volume = (uint8_t)selected;
    settings_store.saveChime(g_chime, g_chime_sound_path);
    boot_options_view.chime_sound_selector.setPreviewVolume(
        g_chime_volumes[g_chime.volume]);
}

static void chime_quiet_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= 2)
        return;
    g_chime.quiet_enabled = selected == 1;
    settings_store.saveChime(g_chime, g_chime_sound_path);
}

static void chime_quiet_start_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected != 0 && selected != 2)
        return;
    g_chime.quiet_start_hour = adjusted_hour(
        g_chime.quiet_start_hour, selected == 0 ? -1 : 1);
    settings_store.saveChime(g_chime, g_chime_sound_path);
    update_chime_options_ui();
}

static void chime_quiet_end_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected != 0 && selected != 2)
        return;
    g_chime.quiet_end_hour = adjusted_hour(
        g_chime.quiet_end_hour, selected == 0 ? -1 : 1);
    settings_store.saveChime(g_chime, g_chime_sound_path);
    update_chime_options_ui();
}

static void night_enabled_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= 2)
        return;
    g_night_mode.enabled = selected == 1;
    settings_store.saveNightMode(g_night_mode);
}

static void night_screen_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= 2)
        return;
    g_night_mode.screen_off_enabled = selected == 1;
    settings_store.saveNightMode(g_night_mode);
}

static void night_start_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected != 0 && selected != 2)
        return;
    g_night_mode.start_hour =
        adjusted_hour(g_night_mode.start_hour, selected == 0 ? -1 : 1);
    settings_store.saveNightMode(g_night_mode);
    update_night_options_ui();
}

static void night_end_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected != 0 && selected != 2)
        return;
    g_night_mode.end_hour =
        adjusted_hour(g_night_mode.end_hour, selected == 0 ? -1 : 1);
    settings_store.saveNightMode(g_night_mode);
    update_night_options_ui();
}

static void night_off_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected != 0 && selected != 2)
        return;
    g_night_mode.screen_off_hour =
        adjusted_hour(
            g_night_mode.screen_off_hour, selected == 0 ? -1 : 1);
    settings_store.saveNightMode(g_night_mode);
    update_night_options_ui();
}

static void wifi_enabled_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= 2)
        return;
    wifi_service.setEnabled(selected == 1);
    update_wifi_options_ui();
}

static void boot_brightness_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected = lv_buttonmatrix_get_selected_button(options);
    if (selected <= static_cast<uint32_t>(BOOT_BRIGHTNESS_HIGHEST))
        apply_boot_brightness((BootBrightness)selected, true);
}

static bool boot_remember_selection()
{
    return boot_options_view.remember_selection &&
           lv_buttonmatrix_has_button_ctrl(
               boot_options_view.remember_selection,
               1, LV_BUTTONMATRIX_CTRL_CHECKED);
}

static void remember_boot_mode(bool emulator)
{
    if (!boot_remember_selection())
        return;
    g_boot_floppy_emulator = emulator;
    settings_store.saveBootMode(emulator);
}

static void boot_start_clock_event(lv_event_t *event)
{
    (void)event;
    remember_boot_mode(false);
    request_state(UI_STATE_EMPTY_SCREEN);
}

static void boot_start_emulator_event(lv_event_t *event)
{
    (void)event;
    remember_boot_mode(true);
    request_state(UI_STATE_EMULATOR);
}

static void boot_diagnostics_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_DIAGNOSTICS);
}

static void boot_wifi_setup_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_WIFI_SETUP);
}

static void wifi_setup_back_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_BOOT_OPTIONS);
}

static void boot_exit_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_NORMAL);
}

static void diagnostics_back_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_BOOT_OPTIONS);
}

void BootOptionsView::setPage(BootOptionsPage page)
{
    if (page >= BOOT_OPTIONS_PAGE_COUNT)
        return;

    const char *page_names[BOOT_OPTIONS_PAGE_COUNT] = {
        tr("Start"), tr("Preferences"), tr("Language"),
        tr("Regional"), tr("Clock Face"), tr("Clock Theme"),
        tr("Screensaver"),
        tr("Night Schedule"), tr("Night Screen"), tr("Chime"),
        tr("Chime Sound"), tr("Chime Volume"), tr("Quiet Hours"),
        tr("Wi-Fi"), tr("Tools")};
    boot_options_view.page = page;
    for (size_t i = 0; i < BOOT_OPTIONS_PAGE_COUNT; ++i)
        lv_obj_add_flag(
            boot_options_view.pages[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(
        boot_options_view.pages[page], LV_OBJ_FLAG_HIDDEN);

    char title[72];
    snprintf(title, sizeof(title), "%s - %s (%u/%u)",
             tr("Boot Options"), page_names[page],
             (unsigned)page + 1,
             (unsigned)BOOT_OPTIONS_PAGE_COUNT);
    lv_label_set_text(boot_options_view.title, title);

    if (page == BOOT_OPTIONS_START)
        lv_obj_add_flag(
            boot_options_view.previous, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(
            boot_options_view.previous, LV_OBJ_FLAG_HIDDEN);

    if (page == BOOT_OPTIONS_TOOLS)
        lv_obj_add_flag(
            boot_options_view.next, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(
            boot_options_view.next, LV_OBJ_FLAG_HIDDEN);
}

static void boot_options_previous_event(lv_event_t *event)
{
    (void)event;
    if (boot_options_view.page > BOOT_OPTIONS_START)
    {
        boot_options_view.setPage(
            (BootOptionsPage)(boot_options_view.page - 1));
    }
}

static void boot_options_next_event(lv_event_t *event)
{
    (void)event;
    if (boot_options_view.page < BOOT_OPTIONS_TOOLS)
    {
        boot_options_view.setPage(
            (BootOptionsPage)(boot_options_view.page + 1));
    }
}

static void boot_options_continue_visual_event(lv_event_t *event)
{
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(event);
    const lv_event_code_t code = lv_event_get_code(event);
    lv_obj_set_style_text_color(
        label,
        code == LV_EVENT_PRESSED ? lv_color_white() : lv_color_black(),
        0);
}

static void style_boot_options_matrix(lv_obj_t *matrix)
{
    const lv_style_selector_t checked_items =
        (lv_style_selector_t)LV_PART_ITEMS |
        (lv_style_selector_t)LV_STATE_CHECKED;
    const lv_style_selector_t pressed_items =
        (lv_style_selector_t)LV_PART_ITEMS |
        (lv_style_selector_t)LV_STATE_PRESSED;

    lv_obj_set_style_bg_opa(matrix, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(matrix, 0, 0);
    lv_obj_set_style_radius(matrix, 0, 0);
    lv_obj_set_style_pad_all(matrix, 0, 0);
    lv_obj_set_style_pad_row(matrix, 10, 0);
    lv_obj_set_style_pad_column(matrix, 10, 0);
    lv_obj_set_style_text_font(matrix, &lv_font_chicago_8, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_border_color(matrix, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(matrix, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(matrix, 4, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(matrix, 0, LV_PART_ITEMS);
    lv_obj_set_style_outline_width(matrix, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, lv_color_black(), checked_items);
    lv_obj_set_style_text_color(matrix, lv_color_white(), checked_items);
    lv_obj_set_style_bg_color(matrix, lv_color_black(), pressed_items);
    lv_obj_set_style_text_color(matrix, lv_color_white(), pressed_items);
}

static lv_obj_t *create_action_button(lv_obj_t *parent,
                                      const char *text,
                                      lv_event_cb_t callback)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_style_bg_color(button, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(button, lv_color_black(), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_radius(button, 4, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_outline_width(button, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_black(), LV_STATE_PRESSED);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(button, boot_options_continue_visual_event,
                        LV_EVENT_PRESSED, label);
    lv_obj_add_event_cb(button, boot_options_continue_visual_event,
                        LV_EVENT_RELEASED, label);
    lv_obj_add_event_cb(button, boot_options_continue_visual_event,
                        LV_EVENT_PRESS_LOST, label);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
    return button;
}

static lv_obj_t *create_boot_options_page(lv_obj_t *parent)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, 276, 130);
    lv_obj_align(page, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    return page;
}

#endif
