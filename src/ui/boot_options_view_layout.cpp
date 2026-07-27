#ifdef MACLOCK_COMBINED_SOURCE
void BootOptionsView::init(lv_obj_t *screen)
{
    update_boot_translation_maps();

    boot_options_view.panel = lv_obj_create(screen);
    lv_obj_set_size(boot_options_view.panel, 292, 208);
    lv_obj_center(boot_options_view.panel);
    lv_obj_set_style_bg_color(boot_options_view.panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(boot_options_view.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(boot_options_view.panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(boot_options_view.panel, 2, 0);
    lv_obj_set_style_radius(boot_options_view.panel, 0, 0);
    lv_obj_set_style_pad_all(boot_options_view.panel, 6, 0);

    boot_options_view.title =
        lv_label_create(boot_options_view.panel);
    lv_label_set_text(boot_options_view.title, tr("Boot Options"));
    lv_obj_set_style_text_font(
        boot_options_view.title, &lv_font_chicago_8, 0);
    lv_obj_align(
        boot_options_view.title, LV_ALIGN_TOP_MID, 0, 0);

    for (size_t i = 0; i < BOOT_OPTIONS_PAGE_COUNT; ++i)
    {
        boot_options_view.pages[i] =
            create_boot_options_page(boot_options_view.panel);
    }

    lv_obj_t *preferences_page =
        boot_options_view.pages[BOOT_OPTIONS_PREFERENCES];
    boot_options_view.brightness_label = lv_label_create(preferences_page);
    lv_label_set_text(boot_options_view.brightness_label, tr("Brightness"));
    lv_obj_set_style_text_font(boot_options_view.brightness_label, &lv_font_chicago_8, 0);
    lv_obj_align(boot_options_view.brightness_label, LV_ALIGN_TOP_MID, 0, 0);

    boot_options_view.brightness_options =
        lv_buttonmatrix_create(preferences_page);
    lv_buttonmatrix_set_map(
        boot_options_view.brightness_options, g_brightness_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.brightness_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.brightness_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        boot_options_view.brightness_options, true);
    lv_obj_set_size(
        boot_options_view.brightness_options, 260, 48);
    lv_obj_align(
        boot_options_view.brightness_options,
        LV_ALIGN_TOP_MID, 0, 13);
    style_boot_options_matrix(boot_options_view.brightness_options);
    lv_obj_add_event_cb(
        boot_options_view.brightness_options, boot_brightness_event,
                        LV_EVENT_VALUE_CHANGED, nullptr);

    boot_options_view.remember_label = lv_label_create(preferences_page);
    lv_label_set_text(boot_options_view.remember_label, tr("Default boot mode"));
    lv_obj_set_style_text_font(boot_options_view.remember_label, &lv_font_chicago_8, 0);
    lv_obj_align(boot_options_view.remember_label, LV_ALIGN_TOP_MID, 0, 67);

    boot_options_view.remember_selection =
        lv_buttonmatrix_create(preferences_page);
    lv_buttonmatrix_set_map(
        boot_options_view.remember_selection, g_remember_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.remember_selection,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.remember_selection,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        boot_options_view.remember_selection, true);
    lv_obj_set_size(
        boot_options_view.remember_selection, 260, 48);
    lv_obj_align(
        boot_options_view.remember_selection,
        LV_ALIGN_BOTTOM_MID, 0, 0);
    style_boot_options_matrix(
        boot_options_view.remember_selection);

    lv_obj_t *language_page =
        boot_options_view.pages[BOOT_OPTIONS_LANGUAGE];
    boot_options_view.language_options =
        lv_list_create(language_page);
    lv_obj_set_size(
        boot_options_view.language_options, 260, 124);
    lv_obj_center(boot_options_view.language_options);
    selector_list_style_container(
        boot_options_view.language_options);
    for (uint32_t i = 0; i < UI_LANGUAGE_COUNT; ++i)
    {
        lv_obj_t *item = lv_list_add_button(
            boot_options_view.language_options, nullptr,
            localization_language_name((UiLanguage)i));
        boot_options_view.language_items[i] = item;
        selector_list_style_item(item);
        lv_obj_set_user_data(item, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(
            item, language_event, LV_EVENT_CLICKED, nullptr);
    }

    lv_obj_t *regional_page =
        boot_options_view.pages[BOOT_OPTIONS_REGIONAL];
    boot_options_view.date_format_label =
        lv_label_create(regional_page);
    lv_label_set_text(
        boot_options_view.date_format_label, tr("Date format"));
    lv_obj_set_style_text_font(
        boot_options_view.date_format_label,
        &lv_font_chicago_8, 0);
    lv_obj_align(
        boot_options_view.date_format_label,
        LV_ALIGN_TOP_MID, 0, 0);

    boot_options_view.date_format_options =
        lv_buttonmatrix_create(regional_page);
    lv_buttonmatrix_set_map(
        boot_options_view.date_format_options,
        g_date_format_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.date_format_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.date_format_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        boot_options_view.date_format_options, true);
    lv_obj_set_size(
        boot_options_view.date_format_options, 260, 42);
    lv_obj_align(
        boot_options_view.date_format_options,
        LV_ALIGN_TOP_MID, 0, 16);
    style_boot_options_matrix(
        boot_options_view.date_format_options);
    lv_obj_set_style_pad_column(
        boot_options_view.date_format_options, 6, 0);
    lv_obj_add_event_cb(
        boot_options_view.date_format_options,
        date_format_event, LV_EVENT_VALUE_CHANGED, nullptr);

    boot_options_view.temperature_unit_label =
        lv_label_create(regional_page);
    lv_label_set_text(
        boot_options_view.temperature_unit_label,
        tr("Temperature unit"));
    lv_obj_set_style_text_font(
        boot_options_view.temperature_unit_label,
        &lv_font_chicago_8, 0);
    lv_obj_align(
        boot_options_view.temperature_unit_label,
        LV_ALIGN_TOP_MID, 0, 69);

    boot_options_view.temperature_unit_options =
        lv_buttonmatrix_create(regional_page);
    lv_buttonmatrix_set_map(
        boot_options_view.temperature_unit_options,
        g_temperature_unit_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.temperature_unit_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.temperature_unit_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        boot_options_view.temperature_unit_options, true);
    lv_obj_set_size(
        boot_options_view.temperature_unit_options, 260, 42);
    lv_obj_align(
        boot_options_view.temperature_unit_options,
        LV_ALIGN_BOTTOM_MID, 0, 0);
    style_boot_options_matrix(
        boot_options_view.temperature_unit_options);
    lv_obj_set_style_pad_column(
        boot_options_view.temperature_unit_options, 10, 0);
    lv_obj_add_event_cb(
        boot_options_view.temperature_unit_options,
        temperature_unit_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *clock_face_page =
        boot_options_view.pages[BOOT_OPTIONS_CLOCK_FACE];
    boot_options_view.clock_face_options =
        lv_buttonmatrix_create(clock_face_page);
    lv_buttonmatrix_set_map(
        boot_options_view.clock_face_options,
        g_clock_face_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.clock_face_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.clock_face_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        boot_options_view.clock_face_options, true);
    lv_obj_set_size(
        boot_options_view.clock_face_options, 260, 124);
    lv_obj_center(boot_options_view.clock_face_options);
    style_boot_options_matrix(
        boot_options_view.clock_face_options);
    lv_obj_add_event_cb(
        boot_options_view.clock_face_options,
        clock_face_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *clock_theme_page =
        boot_options_view.pages[BOOT_OPTIONS_CLOCK_THEME];
    boot_options_view.clock_theme_options =
        lv_buttonmatrix_create(clock_theme_page);
    lv_buttonmatrix_set_map(
        boot_options_view.clock_theme_options,
        g_clock_theme_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.clock_theme_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.clock_theme_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        boot_options_view.clock_theme_options, true);
    lv_obj_set_size(
        boot_options_view.clock_theme_options, 260, 124);
    lv_obj_center(boot_options_view.clock_theme_options);
    style_boot_options_matrix(
        boot_options_view.clock_theme_options);
    lv_obj_add_event_cb(
        boot_options_view.clock_theme_options,
        clock_theme_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *screensaver_page =
        boot_options_view.pages[BOOT_OPTIONS_SCREENSAVER];
    boot_options_view.screensaver_options =
        lv_buttonmatrix_create(screensaver_page);
    lv_buttonmatrix_set_map(
        boot_options_view.screensaver_options,
        g_screensaver_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.screensaver_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.screensaver_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        boot_options_view.screensaver_options, true);
    lv_obj_set_size(
        boot_options_view.screensaver_options, 260, 38);
    lv_obj_align(
        boot_options_view.screensaver_options,
        LV_ALIGN_TOP_MID, 0, 0);
    style_boot_options_matrix(
        boot_options_view.screensaver_options);
    lv_obj_add_event_cb(
        boot_options_view.screensaver_options,
        screensaver_event, LV_EVENT_VALUE_CHANGED, nullptr);

    boot_options_view.screensaver_delay_label =
        lv_label_create(screensaver_page);
    lv_label_set_text(
        boot_options_view.screensaver_delay_label,
        tr("Start after"));
    lv_obj_set_style_text_font(
        boot_options_view.screensaver_delay_label,
        &lv_font_chicago_8, 0);
    lv_obj_align(
        boot_options_view.screensaver_delay_label,
        LV_ALIGN_TOP_MID, 0, 44);

    boot_options_view.screensaver_delay_options =
        lv_buttonmatrix_create(screensaver_page);
    lv_buttonmatrix_set_map(
        boot_options_view.screensaver_delay_options,
        g_screensaver_delay_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.screensaver_delay_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.screensaver_delay_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        boot_options_view.screensaver_delay_options, true);
    lv_obj_set_size(
        boot_options_view.screensaver_delay_options, 260, 64);
    lv_obj_align(
        boot_options_view.screensaver_delay_options,
        LV_ALIGN_BOTTOM_MID, 0, 0);
    style_boot_options_matrix(
        boot_options_view.screensaver_delay_options);
    lv_obj_set_style_pad_row(
        boot_options_view.screensaver_delay_options, 5, 0);
    lv_obj_add_event_cb(
        boot_options_view.screensaver_delay_options,
        screensaver_delay_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *night_schedule_page =
        boot_options_view.pages[BOOT_OPTIONS_NIGHT_SCHEDULE];
    boot_options_view.night_enabled_options =
        lv_buttonmatrix_create(night_schedule_page);
    lv_buttonmatrix_set_map(
        boot_options_view.night_enabled_options, g_night_enabled_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.night_enabled_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.night_enabled_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        boot_options_view.night_enabled_options, true);
    lv_obj_set_size(
        boot_options_view.night_enabled_options, 260, 28);
    lv_obj_align(
        boot_options_view.night_enabled_options,
        LV_ALIGN_TOP_MID, 0, 0);
    style_boot_options_matrix(
        boot_options_view.night_enabled_options);
    lv_obj_add_event_cb(
        boot_options_view.night_enabled_options,
        night_enabled_event, LV_EVENT_VALUE_CHANGED, nullptr);

    boot_options_view.dim_from_label = lv_label_create(night_schedule_page);
    lv_label_set_text(boot_options_view.dim_from_label, tr("Dim from"));
    lv_obj_set_style_text_font(boot_options_view.dim_from_label, &lv_font_chicago_8, 0);
    lv_obj_align(boot_options_view.dim_from_label, LV_ALIGN_TOP_MID, 0, 34);

    boot_options_view.night_start_options =
        lv_buttonmatrix_create(night_schedule_page);
    lv_buttonmatrix_set_map(
        boot_options_view.night_start_options, g_night_start_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.night_start_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_obj_set_size(
        boot_options_view.night_start_options, 260, 28);
    lv_obj_align(
        boot_options_view.night_start_options,
        LV_ALIGN_TOP_MID, 0, 50);
    style_boot_options_matrix(
        boot_options_view.night_start_options);
    lv_obj_set_style_pad_column(
        boot_options_view.night_start_options, 18, 0);
    lv_obj_add_event_cb(
        boot_options_view.night_start_options,
        night_start_event, LV_EVENT_VALUE_CHANGED, nullptr);

    boot_options_view.normal_at_label = lv_label_create(night_schedule_page);
    lv_label_set_text(boot_options_view.normal_at_label, tr("Normal at"));
    lv_obj_set_style_text_font(boot_options_view.normal_at_label, &lv_font_chicago_8, 0);
    lv_obj_align(boot_options_view.normal_at_label, LV_ALIGN_TOP_MID, 0, 84);

    boot_options_view.night_end_options =
        lv_buttonmatrix_create(night_schedule_page);
    lv_buttonmatrix_set_map(
        boot_options_view.night_end_options, g_night_end_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.night_end_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_obj_set_size(
        boot_options_view.night_end_options, 260, 28);
    lv_obj_align(
        boot_options_view.night_end_options,
        LV_ALIGN_BOTTOM_MID, 0, 0);
    style_boot_options_matrix(
        boot_options_view.night_end_options);
    lv_obj_set_style_pad_column(
        boot_options_view.night_end_options, 18, 0);
    lv_obj_add_event_cb(
        boot_options_view.night_end_options,
        night_end_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *night_screen_page =
        boot_options_view.pages[BOOT_OPTIONS_NIGHT_SCREEN];
    boot_options_view.night_screen_options =
        lv_buttonmatrix_create(night_screen_page);
    lv_buttonmatrix_set_map(
        boot_options_view.night_screen_options, g_night_screen_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.night_screen_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.night_screen_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        boot_options_view.night_screen_options, true);
    lv_obj_set_size(
        boot_options_view.night_screen_options, 260, 52);
    lv_obj_align(
        boot_options_view.night_screen_options,
        LV_ALIGN_TOP_MID, 0, 0);
    style_boot_options_matrix(
        boot_options_view.night_screen_options);
    lv_obj_add_event_cb(
        boot_options_view.night_screen_options,
        night_screen_event, LV_EVENT_VALUE_CHANGED, nullptr);

    boot_options_view.screen_off_label = lv_label_create(night_screen_page);
    lv_label_set_text(boot_options_view.screen_off_label, tr("Screen off at"));
    lv_obj_set_style_text_font(boot_options_view.screen_off_label, &lv_font_chicago_8, 0);
    lv_obj_align(boot_options_view.screen_off_label, LV_ALIGN_TOP_MID, 0, 59);

    boot_options_view.night_off_options =
        lv_buttonmatrix_create(night_screen_page);
    lv_buttonmatrix_set_map(
        boot_options_view.night_off_options, g_night_off_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.night_off_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_obj_set_size(
        boot_options_view.night_off_options, 260, 50);
    lv_obj_align(
        boot_options_view.night_off_options,
        LV_ALIGN_BOTTOM_MID, 0, 0);
    style_boot_options_matrix(
        boot_options_view.night_off_options);
    lv_obj_add_event_cb(
        boot_options_view.night_off_options,
        night_off_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *chime_page =
        boot_options_view.pages[BOOT_OPTIONS_CHIME];
    boot_options_view.chime_mode_options =
        lv_buttonmatrix_create(chime_page);
    lv_buttonmatrix_set_map(
        boot_options_view.chime_mode_options, g_chime_mode_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.chime_mode_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.chime_mode_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        boot_options_view.chime_mode_options, true);
    lv_obj_set_size(
        boot_options_view.chime_mode_options, 260, 124);
    lv_obj_center(boot_options_view.chime_mode_options);
    style_boot_options_matrix(
        boot_options_view.chime_mode_options);
    lv_obj_add_event_cb(
        boot_options_view.chime_mode_options,
        chime_mode_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *chime_sound_page =
        boot_options_view.pages[BOOT_OPTIONS_CHIME_SOUND];
    boot_options_view.chime_sound_selector.begin(
        chime_sound_page,
        g_chime_sound_path,
        g_chime_volumes[g_chime.volume],
        chime_sound_changed,
        nullptr);

    lv_obj_t *chime_volume_page =
        boot_options_view.pages[BOOT_OPTIONS_CHIME_VOLUME];
    boot_options_view.chime_volume_options =
        lv_buttonmatrix_create(chime_volume_page);
    lv_buttonmatrix_set_map(
        boot_options_view.chime_volume_options, g_chime_volume_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.chime_volume_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.chime_volume_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        boot_options_view.chime_volume_options, true);
    lv_obj_set_size(
        boot_options_view.chime_volume_options, 260, 124);
    lv_obj_center(boot_options_view.chime_volume_options);
    style_boot_options_matrix(
        boot_options_view.chime_volume_options);
    lv_obj_add_event_cb(
        boot_options_view.chime_volume_options,
        chime_volume_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *chime_quiet_page =
        boot_options_view.pages[BOOT_OPTIONS_CHIME_QUIET];
    boot_options_view.chime_quiet_options =
        lv_buttonmatrix_create(chime_quiet_page);
    lv_buttonmatrix_set_map(
        boot_options_view.chime_quiet_options, g_chime_quiet_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.chime_quiet_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.chime_quiet_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        boot_options_view.chime_quiet_options, true);
    lv_obj_set_size(
        boot_options_view.chime_quiet_options, 260, 28);
    lv_obj_align(
        boot_options_view.chime_quiet_options,
        LV_ALIGN_TOP_MID, 0, 0);
    style_boot_options_matrix(
        boot_options_view.chime_quiet_options);
    lv_obj_add_event_cb(
        boot_options_view.chime_quiet_options,
        chime_quiet_event, LV_EVENT_VALUE_CHANGED, nullptr);

    boot_options_view.quiet_from_label = lv_label_create(chime_quiet_page);
    lv_label_set_text(boot_options_view.quiet_from_label, tr("Quiet from"));
    lv_obj_set_style_text_font(boot_options_view.quiet_from_label, &lv_font_chicago_8, 0);
    lv_obj_align(boot_options_view.quiet_from_label, LV_ALIGN_TOP_MID, 0, 34);

    boot_options_view.chime_quiet_start_options =
        lv_buttonmatrix_create(chime_quiet_page);
    lv_buttonmatrix_set_map(
        boot_options_view.chime_quiet_start_options,
        g_chime_quiet_start_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.chime_quiet_start_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_obj_set_size(
        boot_options_view.chime_quiet_start_options, 260, 28);
    lv_obj_align(
        boot_options_view.chime_quiet_start_options,
        LV_ALIGN_TOP_MID, 0, 50);
    style_boot_options_matrix(
        boot_options_view.chime_quiet_start_options);
    lv_obj_set_style_pad_column(
        boot_options_view.chime_quiet_start_options, 18, 0);
    lv_obj_add_event_cb(
        boot_options_view.chime_quiet_start_options,
        chime_quiet_start_event, LV_EVENT_VALUE_CHANGED, nullptr);

    boot_options_view.quiet_end_label = lv_label_create(chime_quiet_page);
    lv_label_set_text(boot_options_view.quiet_end_label, tr("Quiet ends"));
    lv_obj_set_style_text_font(boot_options_view.quiet_end_label, &lv_font_chicago_8, 0);
    lv_obj_align(boot_options_view.quiet_end_label, LV_ALIGN_TOP_MID, 0, 84);

    boot_options_view.chime_quiet_end_options =
        lv_buttonmatrix_create(chime_quiet_page);
    lv_buttonmatrix_set_map(
        boot_options_view.chime_quiet_end_options,
        g_chime_quiet_end_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.chime_quiet_end_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_obj_set_size(
        boot_options_view.chime_quiet_end_options, 260, 28);
    lv_obj_align(
        boot_options_view.chime_quiet_end_options,
        LV_ALIGN_BOTTOM_MID, 0, 0);
    style_boot_options_matrix(
        boot_options_view.chime_quiet_end_options);
    lv_obj_set_style_pad_column(
        boot_options_view.chime_quiet_end_options, 18, 0);
    lv_obj_add_event_cb(
        boot_options_view.chime_quiet_end_options,
        chime_quiet_end_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *wifi_page =
        boot_options_view.pages[BOOT_OPTIONS_WIFI];
    boot_options_view.wifi_enabled_options =
        lv_buttonmatrix_create(wifi_page);
    lv_buttonmatrix_set_map(
        boot_options_view.wifi_enabled_options, g_wifi_enabled_map);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.wifi_enabled_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.wifi_enabled_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        boot_options_view.wifi_enabled_options, true);
    lv_obj_set_size(
        boot_options_view.wifi_enabled_options, 260, 36);
    lv_obj_align(
        boot_options_view.wifi_enabled_options,
        LV_ALIGN_TOP_MID, 0, 0);
    style_boot_options_matrix(
        boot_options_view.wifi_enabled_options);
    lv_obj_add_event_cb(
        boot_options_view.wifi_enabled_options,
        wifi_enabled_event, LV_EVENT_VALUE_CHANGED, nullptr);

    boot_options_view.wifi_status = lv_label_create(wifi_page);
    lv_label_set_text(
        boot_options_view.wifi_status,
        tr("Wi-Fi disabled\nClock remains fully offline"));
    lv_obj_set_width(boot_options_view.wifi_status, 260);
    lv_obj_set_style_text_font(
        boot_options_view.wifi_status, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_align(
        boot_options_view.wifi_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(
        boot_options_view.wifi_status,
        LV_ALIGN_TOP_MID, 0, 45);

    lv_obj_t *wifi_setup_button =
        create_action_button(
            wifi_page, tr("Setup Wi-Fi"), boot_wifi_setup_event);
    boot_options_view.wifi_setup_label =
        lv_obj_get_child(wifi_setup_button, 0);
    lv_obj_set_size(wifi_setup_button, 260, 46);
    lv_obj_align(
        wifi_setup_button, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *start_page =
        boot_options_view.pages[BOOT_OPTIONS_START];
    lv_obj_t *clock_button =
        create_action_button(start_page, tr("Clock"),
                             boot_start_clock_event);
    boot_options_view.clock_button_label =
        lv_obj_get_child(clock_button, 0);
    lv_label_set_text(
        boot_options_view.clock_button_label, tr("Clock"));
    lv_obj_set_size(clock_button, 122, 124);
    lv_obj_align(clock_button, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *emulator_button =
        create_action_button(start_page, tr("Emulator"),
                             boot_start_emulator_event);
    boot_options_view.emulator_button_label =
        lv_obj_get_child(emulator_button, 0);
    lv_obj_set_size(emulator_button, 122, 124);
    lv_obj_align(emulator_button, LV_ALIGN_RIGHT_MID, -8, 0);

    lv_obj_t *tools_page =
        boot_options_view.pages[BOOT_OPTIONS_TOOLS];
    lv_obj_t *diagnostics_button =
        create_action_button(tools_page, tr("Diagnostics"),
                             boot_diagnostics_event);
    boot_options_view.diagnostics_button_label =
        lv_obj_get_child(diagnostics_button, 0);
    lv_obj_set_size(diagnostics_button, 260, 58);
    lv_obj_align(diagnostics_button, LV_ALIGN_TOP_MID, 0, 0);

    boot_options_view.rtc_status = lv_label_create(tools_page);
    lv_label_set_text(boot_options_view.rtc_status, tr("RTC: checking..."));
    lv_obj_set_width(boot_options_view.rtc_status, 260);
    lv_obj_set_style_text_font(boot_options_view.rtc_status,
                               &lv_font_chicago_8, 0);
    lv_obj_set_style_text_align(boot_options_view.rtc_status,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(
        boot_options_view.rtc_status, LV_ALIGN_TOP_MID, 0, 72);

    lv_obj_t *calibration_label = lv_label_create(tools_page);
    boot_options_view.calibration_label = calibration_label;
    lv_label_set_text(
        calibration_label, tr("Press Clock for screen calibration"));
    lv_obj_set_style_text_font(calibration_label, &lv_font_chicago_8, 0);
    lv_obj_align(calibration_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    boot_options_view.previous =
        create_action_button(
            boot_options_view.panel, tr("Previous"),
            boot_options_previous_event);
    lv_obj_set_size(boot_options_view.previous, 84, 40);
    lv_obj_align(
        boot_options_view.previous,
        LV_ALIGN_BOTTOM_LEFT, 0, 0);
    boot_options_view.previous_label =
        lv_obj_get_child(boot_options_view.previous, 0);

    boot_options_view.exit =
        create_action_button(
            boot_options_view.panel, tr("Exit"),
            boot_exit_event);
    lv_obj_set_size(boot_options_view.exit, 84, 40);
    lv_obj_align(
        boot_options_view.exit,
        LV_ALIGN_BOTTOM_MID, 0, 0);
    boot_options_view.exit_label =
        lv_obj_get_child(boot_options_view.exit, 0);

    boot_options_view.next =
        create_action_button(
            boot_options_view.panel, tr("Next"),
            boot_options_next_event);
    lv_obj_set_size(boot_options_view.next, 84, 40);
    lv_obj_align(
        boot_options_view.next,
        LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    boot_options_view.next_label =
        lv_obj_get_child(boot_options_view.next, 0);

    lv_obj_add_flag(boot_options_view.panel, LV_OBJ_FLAG_HIDDEN);
    boot_options_view.setPage(BOOT_OPTIONS_START);
}

void BootOptionsView::show()
{
    lv_buttonmatrix_clear_button_ctrl_all(boot_options_view.brightness_options,
                                          LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_button_ctrl(boot_options_view.brightness_options,
                                    (uint32_t)g_boot_brightness,
                                    LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_selected_button(
        boot_options_view.brightness_options,
        (uint32_t)g_boot_brightness);

    lv_buttonmatrix_clear_button_ctrl_all(
        boot_options_view.remember_selection,
        LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_button_ctrl(
        boot_options_view.remember_selection,
        1, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_selected_button(
        boot_options_view.remember_selection, 1);
    set_checked_button(
        boot_options_view.date_format_options,
        (uint32_t)g_date_format);
    set_checked_button(
        boot_options_view.temperature_unit_options,
        (uint32_t)g_temperature_unit);
    set_checked_button(
        boot_options_view.clock_face_options,
        (uint32_t)g_clock_face);
    set_checked_button(
        boot_options_view.clock_theme_options,
        (uint32_t)g_clock_theme);
    set_checked_button(
        boot_options_view.screensaver_options,
        (uint32_t)g_screensaver_mode);
    set_checked_button(
        boot_options_view.screensaver_delay_options,
        g_screensaver_delay_index);
    update_language_selection(true);
    update_night_options_ui();
    update_chime_options_ui();
    update_wifi_options_ui();
    boot_options_view.setPage(BOOT_OPTIONS_START);

    char rtc_status[64];
    if (format_rtc_health(rtc_status, sizeof(rtc_status)))
    {
        lv_obj_add_flag(boot_options_view.rtc_status, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_label_set_text(boot_options_view.rtc_status, rtc_status);
        lv_obj_clear_flag(boot_options_view.rtc_status, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(boot_options_view.panel, LV_OBJ_FLAG_HIDDEN);
}

#endif
