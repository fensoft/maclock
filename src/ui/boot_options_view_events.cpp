#ifdef MACLOCK_COMBINED_SOURCE
static const BootOptionsPage
    g_boot_options_section_first[BOOT_OPTIONS_SECTION_COUNT] = {
        BOOT_OPTIONS_LANGUAGE,
        BOOT_OPTIONS_DISPLAY,
        BOOT_OPTIONS_CHIME,
        BOOT_OPTIONS_PREFERENCES};

static const BootOptionsPage
    g_boot_options_section_last[BOOT_OPTIONS_SECTION_COUNT] = {
        BOOT_OPTIONS_DATETIME,
        BOOT_OPTIONS_NIGHT_SCREEN,
        BOOT_OPTIONS_CHIME_QUIET,
        BOOT_OPTIONS_ABOUT};

static bool boot_options_page_position(
    BootOptionsPage page,
    BootOptionsSection *section,
    uint8_t *position,
    uint8_t *page_count)
{
    for (uint8_t i = 0; i < BOOT_OPTIONS_SECTION_COUNT; ++i)
    {
        const BootOptionsPage first =
            g_boot_options_section_first[i];
        const BootOptionsPage last =
            g_boot_options_section_last[i];
        if (page < first || page > last)
            continue;

        if (section)
            *section = static_cast<BootOptionsSection>(i);
        if (position)
            *position = static_cast<uint8_t>(page - first);
        if (page_count)
            *page_count =
                static_cast<uint8_t>(last - first + 1);
        return true;
    }
    return false;
}

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

static void set_checkbox_state(lv_obj_t *checkbox, bool checked)
{
    if (!checkbox)
        return;
    if (checked)
        lv_obj_add_state(checkbox, LV_STATE_CHECKED);
    else
        lv_obj_remove_state(checkbox, LV_STATE_CHECKED);
}

static void update_regional_options_ui()
{
    set_checked_button(
        boot_options_view.date_format_options,
        static_cast<uint32_t>(g_date_format));
    if (boot_options_view.regional_hour_label)
        lv_label_set_text(
            boot_options_view.regional_hour_label,
            g_time_format.hour_format == HourFormat::Hour12
                ? tr("12-hour")
                : tr("24-hour"));
    if (boot_options_view.regional_temperature_label)
        lv_label_set_text(
            boot_options_view.regional_temperature_label,
            g_temperature_unit == UI_TEMPERATURE_FAHRENHEIT
                ? "°F"
                : "°C");
}

static void update_display_options_ui()
{
    if (boot_options_view.leading_zero_checkbox)
        lv_checkbox_set_text(
            boot_options_view.leading_zero_checkbox,
            tr("Initial zero"));
    if (boot_options_view.weekday_checkbox)
        lv_checkbox_set_text(
            boot_options_view.weekday_checkbox,
            tr("Day"));
    if (boot_options_view.seconds_checkbox)
        lv_checkbox_set_text(
            boot_options_view.seconds_checkbox,
            tr("Seconds"));
    if (boot_options_view.dark_mode_checkbox)
        lv_checkbox_set_text(
            boot_options_view.dark_mode_checkbox,
            tr("Dark mode"));
    set_checkbox_state(
        boot_options_view.leading_zero_checkbox,
        g_time_format.leading_zero);
    set_checkbox_state(
        boot_options_view.weekday_checkbox,
        g_time_format.show_weekday);
    set_checkbox_state(
        boot_options_view.seconds_checkbox,
        g_time_format.show_seconds);
    set_checkbox_state(
        boot_options_view.dark_mode_checkbox,
        g_clock_theme == CLOCK_THEME_DARK);
}

static void update_face_customization_options_ui()
{
    set_checked_button(
        boot_options_view.face_accent_options,
        static_cast<uint32_t>(g_face_customization.accent));
    set_checked_button(
        boot_options_view.face_size_options,
        static_cast<uint32_t>(
            g_face_customization.numeral_size));
    set_checked_button(
        boot_options_view.flip_speed_options,
        static_cast<uint32_t>(
            g_face_customization.flip_speed));
    if (boot_options_view.weather_checkbox)
    {
        lv_checkbox_set_text(
            boot_options_view.weather_checkbox,
            tr("Show weather"));
        set_checkbox_state(
            boot_options_view.weather_checkbox,
            g_face_customization.show_weather);
    }
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
    g_face_accent_map[0] = tr("Default");
    g_face_accent_map[1] = tr("Red");
    g_face_accent_map[2] = tr("Orange");
    g_face_accent_map[3] = "\n";
    g_face_accent_map[4] = tr("Green");
    g_face_accent_map[5] = tr("Blue");
    g_face_accent_map[6] = tr("Purple");
    g_face_accent_map[7] = "";
    g_face_size_map[0] = tr("Small");
    g_face_size_map[1] = tr("Default");
    g_face_size_map[2] = tr("Large");
    g_face_size_map[3] = "";
    g_flip_speed_map[0] = tr("Slow");
    g_flip_speed_map[1] = tr("Normal");
    g_flip_speed_map[2] = tr("Fast");
    g_flip_speed_map[3] = "";
    g_screensaver_map[0] = tr("Off");
    g_screensaver_map[1] = tr("After Dark");
    g_screensaver_map[2] = tr("Stars");
    g_screensaver_map[3] = "\n";
    g_screensaver_map[4] = tr("Mac Logo");
    g_screensaver_map[5] = tr("Matrix");
    g_screensaver_map[6] = tr("Pipes");
    g_screensaver_map[7] = "\n";
    g_screensaver_map[8] = tr("Clocks");
    g_screensaver_map[9] = tr("Random");
    g_screensaver_map[10] = "";
    g_screensaver_delay_map[0] = tr("1 min");
    g_screensaver_delay_map[1] = tr("5 min");
    g_screensaver_delay_map[2] = tr("10 min");
    g_screensaver_delay_map[3] = tr("30 min");
    g_screensaver_delay_map[4] = "";
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
        audio_volume_from_index(g_chime.volume));
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

void BootOptionsView::refreshUpdate()
{
    if (!boot_options_view.update_status)
        return;

    const UpdateSnapshot update = update_service.snapshot();
    const bool show_progress =
        update.stage == UpdateStage::DownloadingAssets ||
        update.stage == UpdateStage::InstallingAssets ||
        update.stage == UpdateStage::DownloadingFirmware ||
        update.stage == UpdateStage::UploadingFirmware;
    uint8_t overall_progress = update.progress;
    if (update.stage == UpdateStage::DownloadingAssets ||
        update.stage == UpdateStage::InstallingAssets)
    {
        overall_progress =
            static_cast<uint8_t>(update.progress / 2);
    }
    else if (update.stage == UpdateStage::DownloadingFirmware)
    {
        overall_progress = static_cast<uint8_t>(
            50 + update.progress / 2);
    }
    char status[256];
    char current_version[80];
    char latest_version[80];
    snprintf(
        current_version, sizeof(current_version),
        tr("Current version: %s"),
        update.current_version);
    snprintf(
        latest_version, sizeof(latest_version),
        tr("Latest version: %s"),
        update.latest_version[0]
            ? update.latest_version
            : "-");
    switch (update.stage)
    {
    case UpdateStage::Checking:
        snprintf(
            status, sizeof(status), "%s\n%s",
            tr("Checking for updates..."),
            current_version);
        break;
    case UpdateStage::UpToDate:
        snprintf(
            status, sizeof(status),
            "%s\n%s",
            tr("Maclock is up to date."),
            current_version);
        break;
    case UpdateStage::Available:
        snprintf(
            status, sizeof(status),
            "%s\n%s\n%s",
            current_version, latest_version,
            tr("Update is ready."));
        break;
    case UpdateStage::DownloadingAssets:
    case UpdateStage::InstallingAssets:
    case UpdateStage::DownloadingFirmware:
    case UpdateStage::UploadingFirmware:
        snprintf(
            status, sizeof(status),
            "%s", tr("Installing update..."));
        break;
    case UpdateStage::ReadyToReboot:
        snprintf(
            status, sizeof(status), "%s\n%s",
            tr("Update is ready."),
            latest_version);
        break;
    case UpdateStage::Error:
        snprintf(
            status, sizeof(status), "%s\n%s\n%s",
            tr("Update failed."), current_version,
            update.message);
        break;
    case UpdateStage::Unsupported:
        snprintf(
            status, sizeof(status), "%s",
            tr("Update unavailable."));
        break;
    case UpdateStage::Idle:
    default:
        snprintf(
            status, sizeof(status), "%s",
            current_version);
        break;
    }
    lv_label_set_text(
        boot_options_view.update_status, status);
    lv_bar_set_value(
        boot_options_view.update_progress,
        overall_progress, LV_ANIM_OFF);
    if (show_progress)
        lv_obj_clear_flag(
            boot_options_view.update_progress,
            LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(
            boot_options_view.update_progress,
            LV_OBJ_FLAG_HIDDEN);

    const bool busy = update.busy;
    const bool available =
        update.update_available &&
        update.stage != UpdateStage::ReadyToReboot;
    const bool ready =
        update.stage == UpdateStage::ReadyToReboot;
    lv_label_set_text(
        boot_options_view.update_primary_label,
        ready
            ? tr("Reboot")
            : (available ? tr("Update") : tr("Check Now")));
    lv_label_set_text(
        boot_options_view.update_later_label,
        tr("Later"));
    lv_label_set_text(
        boot_options_view.update_ignore_label,
        tr("Ignore"));

    if (busy)
        lv_obj_add_state(
            boot_options_view.update_primary,
            LV_STATE_DISABLED);
    else
        lv_obj_remove_state(
            boot_options_view.update_primary,
            LV_STATE_DISABLED);

    if (available)
    {
        lv_obj_align(
            boot_options_view.update_primary,
            LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_clear_flag(
            boot_options_view.update_later,
            LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(
            boot_options_view.update_ignore,
            LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_align(
            boot_options_view.update_primary,
            LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_add_flag(
            boot_options_view.update_later,
            LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(
            boot_options_view.update_ignore,
            LV_OBJ_FLAG_HIDDEN);
    }
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

static void apply_time_format_change()
{
    settings_store.saveTimeFormat(g_time_format);
    clock_view.applyTimeFormatLayout();
    clock_view.last_second = -1;
    clock_view.last_update_ms = 0;
    update_regional_options_ui();
    update_display_options_ui();
}

static void date_format_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= UI_DATE_FORMAT_COUNT)
        return;
    g_date_format = static_cast<UiDateFormat>(selected);
    settings_store.saveDateFormat(g_date_format);
    datetime_editor.setDateFormat(g_date_format);
    boot_options_view.refreshDateTime();
    update_regional_options_ui();
}

static void regional_hour_event(lv_event_t *event)
{
    (void)event;
    g_time_format.hour_format =
        g_time_format.hour_format == HourFormat::Hour12
            ? HourFormat::Hour24
            : HourFormat::Hour12;
    apply_time_format_change();
}

static void regional_temperature_event(lv_event_t *event)
{
    (void)event;
    g_temperature_unit =
        g_temperature_unit == UI_TEMPERATURE_FAHRENHEIT
            ? UI_TEMPERATURE_CELSIUS
            : UI_TEMPERATURE_FAHRENHEIT;
    settings_store.saveTemperatureUnit(g_temperature_unit);
    update_regional_options_ui();
}

static void leading_zero_checkbox_event(lv_event_t *event)
{
    lv_obj_t *checkbox =
        (lv_obj_t *)lv_event_get_target(event);
    g_time_format.leading_zero =
        lv_obj_has_state(checkbox, LV_STATE_CHECKED);
    apply_time_format_change();
}

static void weekday_checkbox_event(lv_event_t *event)
{
    lv_obj_t *checkbox =
        (lv_obj_t *)lv_event_get_target(event);
    g_time_format.show_weekday =
        lv_obj_has_state(checkbox, LV_STATE_CHECKED);
    apply_time_format_change();
}

static void seconds_checkbox_event(lv_event_t *event)
{
    lv_obj_t *checkbox =
        (lv_obj_t *)lv_event_get_target(event);
    g_time_format.show_seconds =
        lv_obj_has_state(checkbox, LV_STATE_CHECKED);
    apply_time_format_change();
}

static void dark_mode_checkbox_event(lv_event_t *event)
{
    lv_obj_t *checkbox =
        (lv_obj_t *)lv_event_get_target(event);
    g_clock_theme =
        lv_obj_has_state(checkbox, LV_STATE_CHECKED)
            ? CLOCK_THEME_DARK
            : CLOCK_THEME_LIGHT;
    settings_store.saveClockTheme(g_clock_theme);
    update_display_options_ui();
    if (clock_view.compact)
        clock_view.applyTheme();
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

static void apply_face_customization_change()
{
    settings_store.saveFaceCustomization(
        g_face_customization);
    clock_view.applyFaceCustomization();
    clock_view.last_second = -1;
    clock_view.last_update_ms = 0;
    update_face_customization_options_ui();
}

static void face_accent_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >=
        static_cast<uint8_t>(FaceAccent::Count))
    {
        return;
    }
    g_face_customization.accent =
        static_cast<FaceAccent>(selected);
    apply_face_customization_change();
}

static void face_size_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >=
        static_cast<uint8_t>(FaceNumeralSize::Count))
    {
        return;
    }
    g_face_customization.numeral_size =
        static_cast<FaceNumeralSize>(selected);
    apply_face_customization_change();
}

static void weather_checkbox_event(lv_event_t *event)
{
    lv_obj_t *checkbox =
        (lv_obj_t *)lv_event_get_target(event);
    g_face_customization.show_weather =
        lv_obj_has_state(checkbox, LV_STATE_CHECKED);
    apply_face_customization_change();
}

static void flip_speed_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >=
        static_cast<uint8_t>(FlipAnimationSpeed::Count))
    {
        return;
    }
    g_face_customization.flip_speed =
        static_cast<FlipAnimationSpeed>(selected);
    apply_face_customization_change();
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
    if (selected >= kAudioVolumeLevelCount)
    {
        return;
    }
    g_chime.volume = (uint8_t)selected;
    settings_store.saveChime(g_chime, g_chime_sound_path);
    boot_options_view.chime_sound_selector.setPreviewVolume(
        audio_volume_from_index(g_chime.volume));
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
    boot_options_view.page_on_show = BOOT_OPTIONS_TOOLS;
    request_state(UI_STATE_DIAGNOSTICS);
}

static void boot_wifi_setup_event(lv_event_t *event)
{
    (void)event;
    boot_options_view.page_on_show = BOOT_OPTIONS_WIFI;
    request_state(UI_STATE_WIFI_SETUP);
}

static void boot_update_primary_event(lv_event_t *event)
{
    (void)event;
    const UpdateSnapshot update = update_service.snapshot();
    if (update.busy)
        return;

    if (update.stage == UpdateStage::ReadyToReboot)
        active_app->rebootAfterControlUpdate();
    else if (update.update_available)
        active_app->requestControlUpdateInstall();
    else
        active_app->requestControlUpdateCheck();
    boot_options_view.refreshUpdate();
}

static void boot_update_later_event(lv_event_t *event)
{
    (void)event;
    boot_options_view.standalone_update_prompt = false;
    update_service.dismiss(false);
    request_state(UI_STATE_NORMAL);
}

static void boot_update_ignore_event(lv_event_t *event)
{
    (void)event;
    boot_options_view.standalone_update_prompt = false;
    update_service.dismiss(true);
    request_state(UI_STATE_NORMAL);
}

static void wifi_setup_back_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_BOOT_OPTIONS);
}

static void boot_exit_event(lv_event_t *event)
{
    (void)event;
    if (boot_options_view.page == BOOT_OPTIONS_HOME)
        request_state(UI_STATE_NORMAL);
    else
        boot_options_view.setPage(BOOT_OPTIONS_HOME);
}

static void diagnostics_back_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_BOOT_OPTIONS);
}

static uint8_t datetime_days_in_month(
    uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31};
    if (month == 2)
    {
        const bool leap =
            (year % 4 == 0 && year % 100 != 0) ||
            year % 400 == 0;
        return leap ? 29 : 28;
    }
    return days[month - 1];
}

static void datetime_set_enabled(bool enabled)
{
    lv_obj_t *objects[] = {
        boot_options_view.datetime_fields,
        boot_options_view.datetime_minus,
        boot_options_view.datetime_plus};
    for (lv_obj_t *object : objects)
    {
        if (!object)
            continue;
        if (enabled)
            lv_obj_remove_state(object, LV_STATE_DISABLED);
        else
            lv_obj_add_state(object, LV_STATE_DISABLED);
    }
}

void BootOptionsView::refreshDateTime()
{
    if (!boot_options_view.datetime_fields)
        return;

    if (!rtc_service.available())
    {
        for (uint8_t i = 0;
             i < BOOT_DATETIME_FIELD_COUNT; ++i)
        {
            strlcpy(
                boot_options_view.datetime_text[i], "--",
                sizeof(boot_options_view.datetime_text[i]));
        }
        datetime_set_enabled(false);
    }
    else
    {
        datetime_set_enabled(true);
        const DateTime current = rtc_now();
        snprintf(
            boot_options_view.datetime_text[BOOT_DATETIME_HOUR],
            sizeof(boot_options_view.datetime_text[0]),
            "%s\n%02u", tr("Hour"), current.hour());
        snprintf(
            boot_options_view.datetime_text[BOOT_DATETIME_MINUTE],
            sizeof(boot_options_view.datetime_text[0]),
            "%s\n%02u", tr("Minute"), current.minute());
        snprintf(
            boot_options_view.datetime_text[BOOT_DATETIME_SECOND],
            sizeof(boot_options_view.datetime_text[0]),
            "%s\n%02u", tr("Second"), current.second());
        snprintf(
            boot_options_view.datetime_text[BOOT_DATETIME_DAY],
            sizeof(boot_options_view.datetime_text[0]),
            "%s\n%02u", tr("Day"), current.day());
        snprintf(
            boot_options_view.datetime_text[BOOT_DATETIME_MONTH],
            sizeof(boot_options_view.datetime_text[0]),
            "%s\n%02u", tr("Month"), current.month());
        snprintf(
            boot_options_view.datetime_text[BOOT_DATETIME_YEAR],
            sizeof(boot_options_view.datetime_text[0]),
            "%s\n%04u", tr("Year"), current.year());
    }

    boot_options_view.datetime_field_order[0] =
        BOOT_DATETIME_HOUR;
    boot_options_view.datetime_field_order[1] =
        BOOT_DATETIME_MINUTE;
    boot_options_view.datetime_field_order[2] =
        BOOT_DATETIME_SECOND;
    switch (g_date_format)
    {
    case UI_DATE_FORMAT_MDY:
        boot_options_view.datetime_field_order[3] =
            BOOT_DATETIME_MONTH;
        boot_options_view.datetime_field_order[4] =
            BOOT_DATETIME_DAY;
        boot_options_view.datetime_field_order[5] =
            BOOT_DATETIME_YEAR;
        break;
    case UI_DATE_FORMAT_YMD:
        boot_options_view.datetime_field_order[3] =
            BOOT_DATETIME_YEAR;
        boot_options_view.datetime_field_order[4] =
            BOOT_DATETIME_MONTH;
        boot_options_view.datetime_field_order[5] =
            BOOT_DATETIME_DAY;
        break;
    default:
        boot_options_view.datetime_field_order[3] =
            BOOT_DATETIME_DAY;
        boot_options_view.datetime_field_order[4] =
            BOOT_DATETIME_MONTH;
        boot_options_view.datetime_field_order[5] =
            BOOT_DATETIME_YEAR;
        break;
    }

    for (uint8_t i = 0; i < 3; ++i)
    {
        boot_options_view.datetime_map[i] =
            boot_options_view.datetime_text[
                boot_options_view.datetime_field_order[i]];
        boot_options_view.datetime_map[i + 4] =
            boot_options_view.datetime_text[
                boot_options_view.datetime_field_order[i + 3]];
    }
    boot_options_view.datetime_map[3] = "\n";
    boot_options_view.datetime_map[7] = "";
    lv_buttonmatrix_set_map(
        boot_options_view.datetime_fields,
        boot_options_view.datetime_map);

    uint32_t selected_button = 0;
    for (uint8_t i = 0;
         i < BOOT_DATETIME_FIELD_COUNT; ++i)
    {
        if (boot_options_view.datetime_field_order[i] ==
            boot_options_view.datetime_selected)
        {
            selected_button = i;
            break;
        }
    }
    set_checked_button(
        boot_options_view.datetime_fields, selected_button);
}

static void boot_datetime_field_event(lv_event_t *event)
{
    lv_obj_t *matrix =
        (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(matrix);
    if (selected >= BOOT_DATETIME_FIELD_COUNT)
        return;
    boot_options_view.datetime_selected =
        boot_options_view.datetime_field_order[selected];
    set_checked_button(matrix, selected);
}

static void adjust_boot_datetime(int delta)
{
    if (!rtc_service.available())
        return;

    const DateTime current = rtc_now();
    int year = current.year();
    int month = current.month();
    int day = current.day();
    int hour = current.hour();
    int minute = current.minute();
    int second = current.second();

    switch (boot_options_view.datetime_selected)
    {
    case BOOT_DATETIME_HOUR:
        hour = constrain(hour + delta, 0, 23);
        break;
    case BOOT_DATETIME_MINUTE:
        minute = constrain(minute + delta, 0, 59);
        break;
    case BOOT_DATETIME_SECOND:
        second = constrain(second + delta, 0, 59);
        break;
    case BOOT_DATETIME_DAY:
        day = constrain(
            day + delta, 1,
            (int)datetime_days_in_month(year, month));
        break;
    case BOOT_DATETIME_MONTH:
        month = constrain(month + delta, 1, 12);
        day = min(
            day,
            (int)datetime_days_in_month(year, month));
        break;
    case BOOT_DATETIME_YEAR:
        year = constrain(year + delta, 2000, 2099);
        day = min(
            day,
            (int)datetime_days_in_month(year, month));
        break;
    default:
        return;
    }

    app_events.adjustRtc(
        DateTime(year, month, day, hour, minute, second));
    boot_options_view.refreshDateTime();
}

static void boot_datetime_minus_event(lv_event_t *event)
{
    (void)event;
    adjust_boot_datetime(-1);
}

static void boot_datetime_plus_event(lv_event_t *event)
{
    (void)event;
    adjust_boot_datetime(1);
}

void BootOptionsView::tick(uint32_t now)
{
    if (boot_options_view.page == BOOT_OPTIONS_DATETIME &&
        (!boot_options_view.datetime_last_refresh_ms ||
         now - boot_options_view.datetime_last_refresh_ms >= 250))
    {
        boot_options_view.datetime_last_refresh_ms = now;
        boot_options_view.refreshDateTime();
    }
    else if (
        boot_options_view.page == BOOT_OPTIONS_UPDATE &&
        (!boot_options_view.update_last_refresh_ms ||
         now - boot_options_view.update_last_refresh_ms >= 250))
    {
        boot_options_view.update_last_refresh_ms = now;
        boot_options_view.refreshUpdate();
    }
}

void BootOptionsView::setPage(BootOptionsPage page)
{
    if (page >= BOOT_OPTIONS_PAGE_COUNT)
        return;
    if (page != BOOT_OPTIONS_UPDATE)
        boot_options_view.standalone_update_prompt = false;

    const char *page_names[BOOT_OPTIONS_PAGE_COUNT] = {
        tr("Configuration"), tr("Language"),
        tr("Regional"), tr("Date / Time"), tr("Display"),
        tr("Clock Face"), tr("Face Style"),
        tr("Face Details"), tr("Screensaver"),
        tr("Night Schedule"), tr("Night Screen"), tr("Chime"),
        tr("Chime Sound"), tr("Chime Volume"), tr("Quiet Hours"),
        tr("Preferences"), tr("Start"), tr("Wi-Fi"),
        tr("Tools"), tr("Software Update"), tr("About")};
    const char *section_names[BOOT_OPTIONS_SECTION_COUNT] = {
        tr("General"), tr("Display"), tr("Sound"), tr("System")};
    boot_options_view.page = page;
    for (size_t i = 0; i < BOOT_OPTIONS_PAGE_COUNT; ++i)
        lv_obj_add_flag(
            boot_options_view.pages[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(
        boot_options_view.pages[page], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(
        boot_options_view.pages[BOOT_OPTIONS_UPDATE], 130);
    lv_obj_clear_flag(
        boot_options_view.exit, LV_OBJ_FLAG_HIDDEN);
    if (page == BOOT_OPTIONS_DATETIME)
    {
        boot_options_view.datetime_last_refresh_ms = 0;
        boot_options_view.refreshDateTime();
    }
    else if (page == BOOT_OPTIONS_UPDATE)
    {
        boot_options_view.update_last_refresh_ms = 0;
        boot_options_view.refreshUpdate();
        if (boot_options_view.standalone_update_prompt)
        {
            lv_obj_set_height(
                boot_options_view.pages[BOOT_OPTIONS_UPDATE],
                168);
            lv_label_set_text(
                boot_options_view.title,
                tr("Software Update"));
            lv_obj_add_flag(
                boot_options_view.previous,
                LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(
                boot_options_view.exit,
                LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(
                boot_options_view.next,
                LV_OBJ_FLAG_HIDDEN);
            return;
        }
    }

    if (page == BOOT_OPTIONS_HOME)
    {
        lv_label_set_text(
            boot_options_view.title, tr("Configuration"));
        lv_obj_add_flag(
            boot_options_view.previous, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(
            boot_options_view.next, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(
            boot_options_view.exit_label, tr("Exit"));
        return;
    }

    BootOptionsSection section = BOOT_OPTIONS_SECTION_GENERAL;
    uint8_t position = 0;
    uint8_t page_count = 0;
    if (!boot_options_page_position(
            page, &section, &position, &page_count))
    {
        return;
    }

    char title[72];
    snprintf(
        title, sizeof(title), "%s - %s (%u/%u)",
        section_names[section], page_names[page],
        (unsigned)position + 1, (unsigned)page_count);
    lv_label_set_text(boot_options_view.title, title);
    lv_label_set_text(
        boot_options_view.exit_label, tr("Sections"));

    if (position == 0)
        lv_obj_add_flag(
            boot_options_view.previous, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(
            boot_options_view.previous, LV_OBJ_FLAG_HIDDEN);

    if (position + 1 >= page_count)
        lv_obj_add_flag(
            boot_options_view.next, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(
            boot_options_view.next, LV_OBJ_FLAG_HIDDEN);
}

static void boot_options_previous_event(lv_event_t *event)
{
    (void)event;
    uint8_t position = 0;
    if (boot_options_page_position(
            boot_options_view.page, nullptr, &position, nullptr) &&
        position > 0)
        boot_options_view.setPage(static_cast<BootOptionsPage>(
            boot_options_view.page - 1));
}

static void boot_options_next_event(lv_event_t *event)
{
    (void)event;
    uint8_t position = 0;
    uint8_t page_count = 0;
    if (boot_options_page_position(
            boot_options_view.page, nullptr,
            &position, &page_count) &&
        position + 1 < page_count)
        boot_options_view.setPage(static_cast<BootOptionsPage>(
            boot_options_view.page + 1));
}

static void boot_options_section_event(lv_event_t *event)
{
    lv_obj_t *button =
        (lv_obj_t *)lv_event_get_target(event);
    const uint32_t section =
        (uint32_t)(uintptr_t)lv_obj_get_user_data(button);
    if (section >= BOOT_OPTIONS_SECTION_COUNT)
        return;
    boot_options_view.setPage(
        g_boot_options_section_first[section]);
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

static void hard_pixel_button_draw_event(lv_event_t *event)
{
    lv_draw_task_t *task = lv_event_get_draw_task(event);
    if (!task)
        return;

    lv_draw_dsc_base_t *base =
        static_cast<lv_draw_dsc_base_t *>(
            lv_draw_task_get_draw_dsc(task));
    if (!base ||
        (base->part != LV_PART_MAIN &&
         base->part != LV_PART_ITEMS))
    {
        return;
    }

    if (lv_draw_fill_dsc_t *fill =
            lv_draw_task_get_fill_dsc(task))
    {
        fill->radius = 0;
    }

    lv_draw_border_dsc_t *border =
        lv_draw_task_get_border_dsc(task);
    if (!border)
        return;
    border->radius = 0;

    lv_area_t area;
    lv_draw_task_get_area(task, &area);
    const lv_point_t corners[] = {
        {area.x1, area.y1},
        {area.x2, area.y1},
        {area.x1, area.y2},
        {area.x2, area.y2},
    };
    lv_draw_fill_dsc_t corner;
    lv_draw_fill_dsc_init(&corner);
    corner.color = lv_color_white();
    corner.opa = LV_OPA_COVER;
    for (const lv_point_t &point : corners)
    {
        lv_area_t pixel = {
            point.x, point.y, point.x, point.y};
        lv_draw_fill(base->layer, &corner, &pixel);
    }
}

static void enable_hard_pixel_button_drawing(lv_obj_t *object)
{
    lv_obj_add_flag(
        object, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(
        object, hard_pixel_button_draw_event,
        LV_EVENT_DRAW_TASK_ADDED, nullptr);
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
    lv_obj_set_style_radius(matrix, 1, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(matrix, 0, LV_PART_ITEMS);
    lv_obj_set_style_outline_width(matrix, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, lv_color_black(), checked_items);
    lv_obj_set_style_text_color(matrix, lv_color_white(), checked_items);
    lv_obj_set_style_bg_color(matrix, lv_color_black(), pressed_items);
    lv_obj_set_style_text_color(matrix, lv_color_white(), pressed_items);
    enable_hard_pixel_button_drawing(matrix);
}

static lv_obj_t *create_boot_checkbox(
    lv_obj_t *parent, const char *text,
    lv_event_cb_t callback)
{
    lv_obj_t *checkbox = lv_checkbox_create(parent);
    lv_checkbox_set_text(checkbox, text);
    lv_obj_set_style_text_font(
        checkbox, &lv_font_chicago_8, LV_PART_MAIN);
    lv_obj_set_style_text_color(
        checkbox, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        checkbox, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(
        checkbox, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        checkbox, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(
        checkbox, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(
        checkbox, 1, LV_PART_MAIN);
    lv_obj_set_style_outline_width(
        checkbox, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(
        checkbox, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_top(
        checkbox, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_left(
        checkbox, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_column(
        checkbox, 8, LV_PART_MAIN);

    lv_obj_set_style_pad_all(
        checkbox, 4, LV_PART_INDICATOR);
    lv_obj_set_style_radius(
        checkbox, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(
        checkbox, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(
        checkbox, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_border_color(
        checkbox, lv_color_black(), LV_PART_INDICATOR);
    lv_obj_set_style_border_width(
        checkbox, 1, LV_PART_INDICATOR);
    lv_obj_set_style_shadow_width(
        checkbox, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_image_src(
        checkbox, nullptr, LV_PART_INDICATOR);

    const lv_style_selector_t checked_indicator =
        (lv_style_selector_t)LV_PART_INDICATOR |
        (lv_style_selector_t)LV_STATE_CHECKED;
    lv_obj_set_style_bg_color(
        checkbox, lv_color_white(), checked_indicator);
    lv_obj_set_style_bg_image_src(
        checkbox, LV_SYMBOL_OK, checked_indicator);
    lv_obj_set_style_bg_image_recolor(
        checkbox, lv_color_black(), checked_indicator);
    lv_obj_set_style_bg_image_recolor_opa(
        checkbox, LV_OPA_COVER, checked_indicator);
    lv_obj_set_style_text_font(
        checkbox, LV_FONT_DEFAULT, checked_indicator);
    lv_obj_set_style_text_color(
        checkbox, lv_color_black(), checked_indicator);

    const lv_style_selector_t pressed_main =
        (lv_style_selector_t)LV_PART_MAIN |
        (lv_style_selector_t)LV_STATE_PRESSED;
    lv_obj_set_style_bg_color(
        checkbox, lv_color_black(), pressed_main);
    lv_obj_set_style_text_color(
        checkbox, lv_color_white(), pressed_main);

    lv_obj_set_ext_click_area(checkbox, 0);
    enable_hard_pixel_button_drawing(checkbox);
    lv_obj_add_event_cb(
        checkbox, callback, LV_EVENT_VALUE_CHANGED, nullptr);
    return checkbox;
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
    lv_obj_set_style_radius(button, 1, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_outline_width(button, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_black(), LV_STATE_PRESSED);
    enable_hard_pixel_button_drawing(button);

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
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, 276, 130);
    lv_obj_align(page, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    return page;
}

#endif
