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

static String filesystem_clockface_name(const String &source)
{
    String name;
    for (size_t i = 0; i < source.length() && name.length() <
         AppSettings::kCustomClockFaceNameMax - 1; ++i)
    {
        const char value = source[i];
        if ((value >= 'a' && value <= 'z') ||
            (value >= '0' && value <= '9') || value == '_' || value == '-')
            name += value;
        else
            return String();
    }
    return name;
}

static void clock_face_event(lv_event_t *event);

static void update_clock_face_selection(bool scroll_to_selected)
{
    lv_obj_clean(boot_options_view.clock_face_options);
    boot_options_view.clock_face_count = 0;
    File directory = LittleFS.open("/clockface");
    if (directory && directory.isDirectory())
    {
        for (File entry = directory.openNextFile(); entry &&
             boot_options_view.clock_face_count < kFilesystemClockFaceMax;
             entry = directory.openNextFile())
        {
            if (!entry.isDirectory())
                continue;
            const String path = entry.name();
            const int slash = path.lastIndexOf("/");
            const String name = filesystem_clockface_name(
                slash >= 0 ? path.substring(slash + 1) : path);
            const String project_path = String("/clockface/") +
                name + "/clockface.json";
            File project = name.length()
                ? LittleFS.open(project_path.c_str(), "r") : File();
            if (!project)
                continue;
            JsonDocument document;
            const bool valid = !deserializeJson(document, project);
            project.close();
            if (!valid)
                continue;
            const uint8_t index = boot_options_view.clock_face_count++;
            strlcpy(boot_options_view.clock_face_names[index], name.c_str(),
                sizeof(boot_options_view.clock_face_names[index]));
            JsonVariantConst name_value = document["name"];
            const char *display_name = name_value.is<const char *>()
                ? name_value.as<const char *>() : nullptr;
            strlcpy(boot_options_view.clock_face_labels[index],
                display_name && display_name[0] ? display_name : "Unnamed face",
                sizeof(boot_options_view.clock_face_labels[index]));
            lv_obj_t *item = lv_list_add_button(
                boot_options_view.clock_face_options, nullptr,
                boot_options_view.clock_face_labels[index]);
            boot_options_view.clock_face_items[index] = item;
            selector_list_style_item(item);
            lv_obj_t *item_label = lv_obj_get_child(item, 0);
            if (item_label)
                lv_label_set_text(item_label,
                    boot_options_view.clock_face_labels[index]);
            lv_obj_set_user_data(item, (void *)(uintptr_t)index);
            lv_obj_add_event_cb(item, clock_face_event, LV_EVENT_CLICKED, nullptr);
            if (!strcmp(name.c_str(), app_settings.custom_clock_face))
                lv_obj_add_state(item, LV_STATE_CHECKED);
        }
        directory.close();
    }
    if (scroll_to_selected)
        for (uint8_t i = 0; i < boot_options_view.clock_face_count; ++i)
            if (!strcmp(boot_options_view.clock_face_names[i],
                        app_settings.custom_clock_face))
                lv_obj_scroll_to_view(boot_options_view.clock_face_items[i], LV_ANIM_OFF);
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
    if (boot_options_view.regional_seconds_label)
        lv_label_set_text(boot_options_view.regional_seconds_label,
            g_time_format.show_seconds
                ? "Show seconds: On"
                : "Show seconds: Off");
}

static void update_face_customization_options_ui()
{
    static const char *const speeds[] = {"Slow", "Normal", "Fast"};
    if (boot_options_view.flip_speed_label)
        lv_label_set_text_fmt(boot_options_view.flip_speed_label,
            "Animation: %s", speeds[static_cast<uint8_t>(
                g_face_customization.flip_speed)]);
    if (boot_options_view.colon_blink_label)
        lv_label_set_text(boot_options_view.colon_blink_label,
            g_face_customization.colon_blink == ColonBlinkInterval::None
                ? "Blink: No" : "Blink: Yes");
    if (boot_options_view.continuous_seconds_label)
        lv_label_set_text(boot_options_view.continuous_seconds_label,
            g_face_customization.continuous_seconds
                ? "Continuous: Yes" : "Continuous: No");
}

static void update_boot_translation_maps()
{
    g_brightness_map[0] = tr("Latest");
    g_brightness_map[1] = tr("Lowest");
    g_brightness_map[2] = tr("Highest");
    g_brightness_map[3] = "";
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
    clock_view.last_second = -1;
    clock_view.last_update_ms = 0;
    update_regional_options_ui();
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

static void regional_seconds_event(lv_event_t *event)
{
    (void)event;
    g_time_format.show_seconds = !g_time_format.show_seconds;
    apply_time_format_change();
}

static void clock_face_event(lv_event_t *event)
{
    lv_obj_t *item = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        (uint32_t)(uintptr_t)lv_obj_get_user_data(item);
    if (selected >= boot_options_view.clock_face_count)
        return;
    strlcpy(app_settings.custom_clock_face,
        boot_options_view.clock_face_names[selected],
        sizeof(app_settings.custom_clock_face));
    settings_store.saveCustomClockFace(app_settings.custom_clock_face);
    update_clock_face_selection(false);
}

static void apply_face_customization_change()
{
    settings_store.saveFaceCustomization(
        g_face_customization);
    clock_view.last_second = -1;
    clock_view.last_update_ms = 0;
    update_face_customization_options_ui();
}

static void flip_speed_event(lv_event_t *event)
{
    (void)event;
    g_face_customization.flip_speed =
        static_cast<FlipAnimationSpeed>((static_cast<uint8_t>(
            g_face_customization.flip_speed) + 1) %
            static_cast<uint8_t>(FlipAnimationSpeed::Count));
    apply_face_customization_change();
}


static void colon_blink_event(lv_event_t *event)
{
    (void)event;
    g_face_customization.colon_blink =
        g_face_customization.colon_blink == ColonBlinkInterval::None
            ? ColonBlinkInterval::OneSecond : ColonBlinkInterval::None;
    apply_face_customization_change();
}

static void continuous_seconds_event(lv_event_t *event)
{
    (void)event;
    g_face_customization.continuous_seconds =
        !g_face_customization.continuous_seconds;
    apply_face_customization_change();
}

static const char *screensaver_mode_text(ScreensaverMode mode)
{
    static const char *names[] = {
        "Off", "After Dark", "Stars", "Mac Logo", "Matrix", "Pipes",
        "Clocks", "Random", "Flying Toasters", "Marquee Message",
        "Digital Rain Clock", "Mystify", "Aquarium", "Game of Life",
        "Maze", "Error Parade", "Rainy Window", "Fireworks",
        "Photo Slideshow"};
    const uint8_t index = static_cast<uint8_t>(mode);
    return tr(index < SCREENSAVER_MODE_COUNT ? names[index] : names[0]);
}

static void update_screensaver_mode_button(bool scroll_to_selected = false)
{
    const uint8_t selected =
        static_cast<uint8_t>(g_screensaver_mode);
    for (uint8_t i = 0; i < SCREENSAVER_MODE_COUNT; ++i)
    {
        lv_obj_t *item = boot_options_view.screensaver_items[i];
        if (!item)
            continue;
        lv_obj_t *label = lv_obj_get_child(item, 0);
        if (label)
        {
            lv_label_set_text(
                label,
                screensaver_mode_text(
                    static_cast<ScreensaverMode>(i)));
        }
        if (i == selected)
            lv_obj_add_state(item, LV_STATE_CHECKED);
        else
            lv_obj_remove_state(item, LV_STATE_CHECKED);
    }

    if (scroll_to_selected && selected < SCREENSAVER_MODE_COUNT &&
        boot_options_view.screensaver_items[selected])
    {
        lv_obj_scroll_to_view(
            boot_options_view.screensaver_items[selected],
            LV_ANIM_OFF);
    }
}

static void screensaver_event(lv_event_t *event)
{
    lv_obj_t *item = (lv_obj_t *)lv_event_get_target(event);
    const uint8_t selected = static_cast<uint8_t>(
        (uintptr_t)lv_obj_get_user_data(item));
    if (selected >= SCREENSAVER_MODE_COUNT)
        return;
    g_screensaver_mode = static_cast<ScreensaverMode>(selected);
    settings_store.saveScreensaverMode(g_screensaver_mode);
    update_screensaver_mode_button(false);
}

static void screensaver_delay_event(lv_event_t *event)
{
    lv_obj_t *item = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        (uint32_t)(uintptr_t)lv_obj_get_user_data(item);
    if (selected >=
        sizeof(g_screensaver_delays_minutes) /
            sizeof(g_screensaver_delays_minutes[0]))
    {
        return;
    }
    g_screensaver_delay_index = (uint8_t)selected;
    settings_store.saveScreensaverDelay(
        g_screensaver_delay_index);

    for (uint32_t i = 0; i < kScreensaverDelayCount; ++i)
    {
        if (boot_options_view.screensaver_delay_items[i])
        {
            if (i == selected)
            {
                lv_obj_add_state(
                    boot_options_view.screensaver_delay_items[i],
                    LV_STATE_CHECKED);
            }
            else
            {
                lv_obj_remove_state(
                    boot_options_view.screensaver_delay_items[i],
                    LV_STATE_CHECKED);
            }
        }
    }
}

static void update_screensaver_delay_selection(
    bool scroll_to_selected)
{
    for (uint32_t i = 0; i < kScreensaverDelayCount; ++i)
    {
        lv_obj_t *item =
            boot_options_view.screensaver_delay_items[i];
        if (!item)
            continue;
        lv_obj_t *label = lv_obj_get_child(item, 0);
        if (label)
            lv_label_set_text(label, g_screensaver_delay_map[i]);
        if (i == g_screensaver_delay_index)
            lv_obj_add_state(item, LV_STATE_CHECKED);
        else
            lv_obj_remove_state(item, LV_STATE_CHECKED);
    }

    if (scroll_to_selected &&
        g_screensaver_delay_index < kScreensaverDelayCount &&
        boot_options_view.screensaver_delay_items[
            g_screensaver_delay_index])
    {
        lv_obj_scroll_to_view(
            boot_options_view.screensaver_delay_items[
                g_screensaver_delay_index],
            LV_ANIM_OFF);
    }
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

#endif
