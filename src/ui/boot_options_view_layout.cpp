#ifdef MACLOCK_COMBINED_SOURCE
void BootOptionsView::init(lv_obj_t *screen)
{
    update_boot_translation_maps();

    boot_options_view.panel = lv_obj_create(screen);
    lv_obj_remove_flag(
        boot_options_view.panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(boot_options_view.panel, 292, 208);
    lv_obj_center(boot_options_view.panel);
    lv_obj_set_style_bg_color(boot_options_view.panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(boot_options_view.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(boot_options_view.panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(boot_options_view.panel, 2, 0);
    lv_obj_set_style_radius(boot_options_view.panel, 0, 0);
    lv_obj_set_style_pad_all(boot_options_view.panel, 6, 0);

    boot_options_view.title_bar =
        lv_obj_create(boot_options_view.panel);
    lv_obj_remove_style_all(
        boot_options_view.title_bar);
    lv_obj_remove_flag(
        boot_options_view.title_bar,
        LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(
        boot_options_view.title_bar, 276, 15);
    lv_obj_align(
        boot_options_view.title_bar,
        LV_ALIGN_TOP_MID, 0, -3);

    for (uint8_t y = 1; y <= 11; y += 2)
    {
        lv_obj_t *stripe =
            lv_obj_create(boot_options_view.title_bar);
        lv_obj_remove_style_all(stripe);
        lv_obj_remove_flag(
            stripe, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(stripe, lv_pct(100), 1);
        lv_obj_set_pos(stripe, 0, y);
        lv_obj_set_style_bg_color(
            stripe, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(
            stripe, LV_OPA_COVER, 0);
    }

    boot_options_view.title_close =
        lv_obj_create(boot_options_view.title_bar);
    lv_obj_remove_style_all(
        boot_options_view.title_close);
    lv_obj_remove_flag(
        boot_options_view.title_close,
        LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(
        boot_options_view.title_close, 13, 11);
    lv_obj_set_pos(
        boot_options_view.title_close, 6, 1);
    lv_obj_set_style_bg_color(
        boot_options_view.title_close,
        lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        boot_options_view.title_close,
        LV_OPA_COVER, 0);

    lv_obj_t *close_box =
        lv_obj_create(boot_options_view.title_close);
    lv_obj_remove_flag(
        close_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(close_box, 11, 11);
    lv_obj_set_pos(close_box, 1, 0);
    lv_obj_set_style_bg_color(
        close_box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        close_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        close_box,
        lv_color_black(), 0);
    lv_obj_set_style_border_width(
        close_box, 1, 0);
    lv_obj_set_style_radius(
        close_box, 0, 0);
    lv_obj_set_style_pad_all(
        close_box, 0, 0);

    boot_options_view.title_zoom =
        lv_obj_create(boot_options_view.title_bar);
    lv_obj_remove_style_all(
        boot_options_view.title_zoom);
    lv_obj_remove_flag(
        boot_options_view.title_zoom,
        LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(
        boot_options_view.title_zoom, 13, 11);
    lv_obj_align(
        boot_options_view.title_zoom,
        LV_ALIGN_TOP_RIGHT, -6, 1);
    lv_obj_set_style_bg_color(
        boot_options_view.title_zoom,
        lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        boot_options_view.title_zoom,
        LV_OPA_COVER, 0);

    lv_obj_t *zoom_box =
        lv_obj_create(boot_options_view.title_zoom);
    lv_obj_remove_flag(
        zoom_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(zoom_box, 11, 11);
    lv_obj_set_pos(zoom_box, 1, 0);
    lv_obj_set_style_bg_color(
        zoom_box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        zoom_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        zoom_box,
        lv_color_black(), 0);
    lv_obj_set_style_border_width(
        zoom_box, 1, 0);
    lv_obj_set_style_radius(
        zoom_box, 0, 0);
    lv_obj_set_style_pad_all(
        zoom_box, 0, 0);

    lv_obj_t *zoom_inset =
        lv_obj_create(boot_options_view.title_zoom);
    lv_obj_remove_style_all(zoom_inset);
    lv_obj_remove_flag(
        zoom_inset, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(zoom_inset, 7, 7);
    lv_obj_set_pos(zoom_inset, 1, 0);
    lv_obj_set_style_border_color(
        zoom_inset, lv_color_black(), 0);
    lv_obj_set_style_border_width(
        zoom_inset, 1, 0);
    lv_obj_set_style_bg_opa(
        zoom_inset, LV_OPA_TRANSP, 0);

    lv_obj_t *title_separator =
        lv_obj_create(boot_options_view.panel);
    lv_obj_remove_style_all(title_separator);
    lv_obj_remove_flag(
        title_separator, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(title_separator, 292, 1);
    lv_obj_align(
        title_separator, LV_ALIGN_TOP_MID, 0, 13);
    lv_obj_set_style_bg_color(
        title_separator, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(
        title_separator, LV_OPA_COVER, 0);

    boot_options_view.title =
        lv_label_create(boot_options_view.title_bar);
    lv_label_set_text(boot_options_view.title, tr("Configuration"));
    lv_obj_set_style_text_font(
        boot_options_view.title, &lv_font_chicago_8, 0);
    lv_obj_set_style_bg_color(
        boot_options_view.title, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        boot_options_view.title, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(
        boot_options_view.title, 5, 0);
    lv_obj_set_style_pad_right(
        boot_options_view.title, 5, 0);
    lv_obj_set_style_pad_top(
        boot_options_view.title, 1, 0);
    lv_obj_set_style_pad_bottom(
        boot_options_view.title, 1, 0);
    lv_obj_align(
        boot_options_view.title,
        LV_ALIGN_TOP_MID, 0, -2);
    lv_obj_move_foreground(
        boot_options_view.title_close);
    lv_obj_move_foreground(
        boot_options_view.title_zoom);
    lv_obj_move_foreground(
        boot_options_view.title);

    for (size_t i = 0; i < BOOT_OPTIONS_PAGE_COUNT; ++i)
    {
        boot_options_view.pages[i] =
            create_boot_options_page(boot_options_view.panel);
    }

    lv_obj_t *home_page =
        boot_options_view.pages[BOOT_OPTIONS_HOME];
    const char *section_names[BOOT_OPTIONS_SECTION_COUNT] = {
        tr("General"), tr("Display"), tr("Sound"), tr("System")};
    const lv_align_t section_alignments[BOOT_OPTIONS_SECTION_COUNT] = {
        LV_ALIGN_TOP_LEFT, LV_ALIGN_TOP_RIGHT,
        LV_ALIGN_BOTTOM_LEFT, LV_ALIGN_BOTTOM_RIGHT};
    const int16_t section_x[BOOT_OPTIONS_SECTION_COUNT] = {
        4, -4, 4, -4};
    for (uint8_t i = 0; i < BOOT_OPTIONS_SECTION_COUNT; ++i)
    {
        lv_obj_t *button =
            create_action_button(
                home_page, section_names[i],
                boot_options_section_event);
        boot_options_view.section_labels[i] =
            lv_obj_get_child(button, 0);
        lv_obj_set_user_data(
            button, (void *)(uintptr_t)i);
        lv_obj_set_size(button, 130, 52);
        lv_obj_align(
            button, section_alignments[i],
            section_x[i], 0);
    }
    boot_options_view.home_calibration_label =
        lv_label_create(home_page);
    lv_label_set_text(
        boot_options_view.home_calibration_label,
        tr("Click again for calibration"));
    lv_obj_set_style_text_font(
        boot_options_view.home_calibration_label,
        &lv_font_chicago_8, 0);
    lv_obj_set_style_bg_color(
        boot_options_view.home_calibration_label,
        lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        boot_options_view.home_calibration_label,
        LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(
        boot_options_view.home_calibration_label, 4, 0);
    lv_obj_set_style_pad_right(
        boot_options_view.home_calibration_label, 4, 0);
    lv_obj_set_style_pad_top(
        boot_options_view.home_calibration_label, 1, 0);
    lv_obj_set_style_pad_bottom(
        boot_options_view.home_calibration_label, 1, 0);
    lv_obj_center(
        boot_options_view.home_calibration_label);

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
        boot_options_view.date_format_options, 260, 50);
    lv_obj_align(
        boot_options_view.date_format_options,
        LV_ALIGN_TOP_MID, 0, 0);
    style_boot_options_matrix(
        boot_options_view.date_format_options);
    lv_obj_set_style_pad_column(
        boot_options_view.date_format_options, 6, 0);
    lv_obj_add_event_cb(
        boot_options_view.date_format_options,
        date_format_event, LV_EVENT_VALUE_CHANGED, nullptr);

    boot_options_view.regional_hour_button =
        create_action_button(
            regional_page,
            g_time_format.hour_format == HourFormat::Hour12
                ? tr("12-hour")
                : tr("24-hour"),
            regional_hour_event);
    lv_obj_set_size(
        boot_options_view.regional_hour_button, 126, 30);
    lv_obj_align(boot_options_view.regional_hour_button,
        LV_ALIGN_TOP_LEFT, 0, 48);
    boot_options_view.regional_hour_label =
        lv_obj_get_child(
            boot_options_view.regional_hour_button, 0);

    boot_options_view.regional_temperature_button =
        create_action_button(
            regional_page,
            g_temperature_unit == UI_TEMPERATURE_FAHRENHEIT
                ? "°F"
                : "°C",
            regional_temperature_event);
    lv_obj_set_size(
        boot_options_view.regional_temperature_button, 126, 30);
    lv_obj_align(boot_options_view.regional_temperature_button,
        LV_ALIGN_TOP_RIGHT, 0, 48);
    boot_options_view.regional_temperature_label =
        lv_obj_get_child(
            boot_options_view.regional_temperature_button, 0);

    lv_obj_t *datetime_page =
        boot_options_view.pages[BOOT_OPTIONS_DATETIME];
    boot_options_view.datetime_fields =
        lv_buttonmatrix_create(datetime_page);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.datetime_fields,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        boot_options_view.datetime_fields,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        boot_options_view.datetime_fields, true);
    lv_obj_set_size(
        boot_options_view.datetime_fields, 260, 76);
    lv_obj_align(
        boot_options_view.datetime_fields,
        LV_ALIGN_TOP_MID, 0, 0);
    style_boot_options_matrix(
        boot_options_view.datetime_fields);
    lv_obj_set_style_pad_row(
        boot_options_view.datetime_fields, 6, 0);
    lv_obj_set_style_pad_column(
        boot_options_view.datetime_fields, 6, 0);
    lv_obj_add_event_cb(
        boot_options_view.datetime_fields,
        boot_datetime_field_event,
        LV_EVENT_VALUE_CHANGED, nullptr);

    boot_options_view.datetime_minus =
        create_action_button(
            datetime_page, "-",
            boot_datetime_minus_event);
    lv_obj_set_size(
        boot_options_view.datetime_minus, 126, 46);
    lv_obj_align(
        boot_options_view.datetime_minus,
        LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(
        boot_options_view.datetime_minus,
        boot_datetime_minus_event,
        LV_EVENT_LONG_PRESSED_REPEAT, nullptr);

    boot_options_view.datetime_plus =
        create_action_button(
            datetime_page, "+",
            boot_datetime_plus_event);
    lv_obj_set_size(
        boot_options_view.datetime_plus, 126, 46);
    lv_obj_align(
        boot_options_view.datetime_plus,
        LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(
        boot_options_view.datetime_plus,
        boot_datetime_plus_event,
        LV_EVENT_LONG_PRESSED_REPEAT, nullptr);

    boot_options_view.refreshDateTime();

    lv_obj_t *clock_face_page =
        boot_options_view.pages[BOOT_OPTIONS_CLOCK_FACE];
    boot_options_view.clock_face_options =
        lv_list_create(clock_face_page);
    lv_obj_set_size(
        boot_options_view.clock_face_options, 260, 124);
    lv_obj_center(boot_options_view.clock_face_options);
    selector_list_style_container(
        boot_options_view.clock_face_options);

    lv_obj_t *face_settings_page =
        boot_options_view.pages[BOOT_OPTIONS_FACE_SETTINGS];

    boot_options_view.flip_speed_options = create_action_button(
        face_settings_page, "Animation: Normal", flip_speed_event);
    lv_obj_set_size(boot_options_view.flip_speed_options, 126, 58);
    lv_obj_align(boot_options_view.flip_speed_options, LV_ALIGN_TOP_LEFT, 0, 0);
    boot_options_view.flip_speed_label = lv_obj_get_child(
        boot_options_view.flip_speed_options, 0);

    boot_options_view.colon_blink_options = create_action_button(
        face_settings_page, "Blink: Yes", colon_blink_event);
    lv_obj_set_size(boot_options_view.colon_blink_options, 126, 58);
    lv_obj_align(boot_options_view.colon_blink_options, LV_ALIGN_TOP_RIGHT, 0, 0);
    boot_options_view.colon_blink_label = lv_obj_get_child(
        boot_options_view.colon_blink_options, 0);

    boot_options_view.continuous_seconds_options = create_action_button(
        face_settings_page, "Continuous: No", continuous_seconds_event);
    lv_obj_set_size(boot_options_view.continuous_seconds_options, 126, 58);
    lv_obj_align(boot_options_view.continuous_seconds_options, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    boot_options_view.continuous_seconds_label = lv_obj_get_child(
        boot_options_view.continuous_seconds_options, 0);

    boot_options_view.regional_seconds_button = create_action_button(
        face_settings_page, "Show seconds: No", regional_seconds_event);
    lv_obj_set_size(boot_options_view.regional_seconds_button, 126, 58);
    lv_obj_align(boot_options_view.regional_seconds_button, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    boot_options_view.regional_seconds_label = lv_obj_get_child(
        boot_options_view.regional_seconds_button, 0);

    lv_obj_t *screensaver_page =
        boot_options_view.pages[BOOT_OPTIONS_SCREENSAVER];
    boot_options_view.screensaver_options =
        lv_list_create(screensaver_page);
    lv_obj_set_size(
        boot_options_view.screensaver_options, 126, 124);
    lv_obj_align(
        boot_options_view.screensaver_options,
        LV_ALIGN_LEFT_MID, 0, 0);
    selector_list_style_container(
        boot_options_view.screensaver_options);
    for (uint8_t i = 0; i < SCREENSAVER_MODE_COUNT; ++i)
    {
        lv_obj_t *item = lv_list_add_button(
            boot_options_view.screensaver_options, nullptr,
            screensaver_mode_text(
                static_cast<ScreensaverMode>(i)));
        boot_options_view.screensaver_items[i] = item;
        selector_list_style_item(item);
        lv_obj_set_user_data(item, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(
            item, screensaver_event, LV_EVENT_CLICKED, nullptr);
    }
    update_screensaver_mode_button(true);

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
        LV_ALIGN_TOP_RIGHT, -26, 0);

    boot_options_view.screensaver_delay_options =
        lv_list_create(screensaver_page);
    lv_obj_set_size(
        boot_options_view.screensaver_delay_options, 126, 105);
    lv_obj_align(
        boot_options_view.screensaver_delay_options,
        LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    selector_list_style_container(
        boot_options_view.screensaver_delay_options);
    for (uint32_t i = 0; i < kScreensaverDelayCount; ++i)
    {
        lv_obj_t *item = lv_list_add_button(
            boot_options_view.screensaver_delay_options, nullptr,
            g_screensaver_delay_map[i]);
        boot_options_view.screensaver_delay_items[i] = item;
        selector_list_style_item(item);
        lv_obj_set_user_data(item, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(
            item, screensaver_delay_event,
            LV_EVENT_CLICKED, nullptr);
    }
    update_screensaver_delay_selection(true);

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
        audio_volume_from_index(g_chime.volume),
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
    lv_obj_set_size(clock_button, 122, 82);
    lv_obj_align(clock_button, LV_ALIGN_TOP_LEFT, 8, 0);

    lv_obj_t *emulator_button =
        create_action_button(start_page, tr("Emulator"),
                             boot_start_emulator_event);
    boot_options_view.emulator_button_label =
        lv_obj_get_child(emulator_button, 0);
    lv_obj_set_size(emulator_button, 122, 82);
    lv_obj_align(emulator_button, LV_ALIGN_TOP_RIGHT, -8, 0);

    lv_obj_t *boot_mode_button =
        create_action_button(
            start_page, "", boot_mode_toggle_event);
    boot_options_view.boot_mode_button_label =
        lv_obj_get_child(boot_mode_button, 0);
    lv_obj_set_size(boot_mode_button, 260, 38);
    lv_obj_align(boot_mode_button, LV_ALIGN_BOTTOM_MID, 0, 0);
    update_boot_mode_button();

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

    lv_obj_t *update_page =
        boot_options_view.pages[BOOT_OPTIONS_UPDATE];
    boot_options_view.update_status =
        lv_label_create(update_page);
    lv_label_set_text(
        boot_options_view.update_status,
        tr("Checking for updates..."));
    lv_label_set_long_mode(
        boot_options_view.update_status,
        LV_LABEL_LONG_DOT);
    lv_obj_set_size(
        boot_options_view.update_status, 260, 56);
    lv_obj_set_style_text_font(
        boot_options_view.update_status,
        &lv_font_chicago_8, 0);
    lv_obj_set_style_text_align(
        boot_options_view.update_status,
        LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(
        boot_options_view.update_status, 3, 0);
    lv_obj_align(
        boot_options_view.update_status,
        LV_ALIGN_TOP_MID, 0, 4);

    boot_options_view.update_progress =
        lv_bar_create(update_page);
    lv_bar_set_range(
        boot_options_view.update_progress, 0, 100);
    lv_bar_set_value(
        boot_options_view.update_progress, 0, LV_ANIM_OFF);
    lv_obj_set_size(
        boot_options_view.update_progress, 220, 14);
    lv_obj_align(
        boot_options_view.update_progress,
        LV_ALIGN_TOP_MID, 0, 62);
    lv_obj_set_style_bg_color(
        boot_options_view.update_progress,
        lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(
        boot_options_view.update_progress,
        LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        boot_options_view.update_progress,
        lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(
        boot_options_view.update_progress,
        1, LV_PART_MAIN);
    lv_obj_set_style_radius(
        boot_options_view.update_progress,
        0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(
        boot_options_view.update_progress,
        2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        boot_options_view.update_progress,
        lv_color_black(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(
        boot_options_view.update_progress,
        LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(
        boot_options_view.update_progress,
        0, LV_PART_INDICATOR);
    lv_obj_add_flag(
        boot_options_view.update_progress,
        LV_OBJ_FLAG_HIDDEN);

    boot_options_view.update_primary =
        create_action_button(
            update_page, tr("Check Now"),
            boot_update_primary_event);
    boot_options_view.update_primary_label =
        lv_obj_get_child(
            boot_options_view.update_primary, 0);
    lv_obj_set_size(
        boot_options_view.update_primary, 84, 40);
    lv_obj_align(
        boot_options_view.update_primary,
        LV_ALIGN_BOTTOM_MID, 0, 0);

    boot_options_view.update_later =
        create_action_button(
            update_page, tr("Later"),
            boot_update_later_event);
    boot_options_view.update_later_label =
        lv_obj_get_child(
            boot_options_view.update_later, 0);
    lv_obj_set_size(
        boot_options_view.update_later, 84, 40);
    lv_obj_align(
        boot_options_view.update_later,
        LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(
        boot_options_view.update_later,
        LV_OBJ_FLAG_HIDDEN);

    boot_options_view.update_ignore =
        create_action_button(
            update_page, tr("Ignore"),
            boot_update_ignore_event);
    boot_options_view.update_ignore_label =
        lv_obj_get_child(
            boot_options_view.update_ignore, 0);
    lv_obj_set_size(
        boot_options_view.update_ignore, 84, 40);
    lv_obj_align(
        boot_options_view.update_ignore,
        LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_flag(
        boot_options_view.update_ignore,
        LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *about_page =
        boot_options_view.pages[BOOT_OPTIONS_ABOUT];

    boot_options_view.about_author =
        lv_label_create(about_page);
    lv_label_set_text(
        boot_options_view.about_author,
        tr("Author: fensoft"));
    lv_obj_set_style_text_font(
        boot_options_view.about_author,
        &lv_font_chicago_8, 0);
    lv_obj_align(
        boot_options_view.about_author,
        LV_ALIGN_TOP_LEFT, 0, 0);

    boot_options_view.about_link =
        lv_label_create(about_page);
    lv_label_set_text(
        boot_options_view.about_link,
        "github.com/fensoft/\nmaclock");
    lv_obj_set_style_text_font(
        boot_options_view.about_link,
        &lv_font_chicago_8, 0);
    lv_obj_set_style_text_line_space(
        boot_options_view.about_link, 2, 0);
    lv_obj_align(
        boot_options_view.about_link,
        LV_ALIGN_TOP_LEFT, 0, 18);

    boot_options_view.about_logo =
        lv_image_create(about_page);
    lv_image_set_src(
        boot_options_view.about_logo, &fensoft_logo);
    lv_obj_set_size(
        boot_options_view.about_logo, 64, 64);
    lv_image_set_inner_align(
        boot_options_view.about_logo, LV_IMAGE_ALIGN_CONTAIN);
    lv_obj_align(
        boot_options_view.about_logo,
        LV_ALIGN_TOP_LEFT, 4, 56);

    boot_options_view.about_qr =
        lv_qrcode_create(about_page);
    lv_qrcode_set_size(
        boot_options_view.about_qr, 112);
    lv_qrcode_set_dark_color(
        boot_options_view.about_qr, lv_color_black());
    lv_qrcode_set_light_color(
        boot_options_view.about_qr, lv_color_white());
    lv_qrcode_set_quiet_zone(
        boot_options_view.about_qr, true);
    lv_qrcode_set_data(
        boot_options_view.about_qr,
        "https://github.com/fensoft/maclock");
    lv_obj_align(
        boot_options_view.about_qr,
        LV_ALIGN_RIGHT_MID, 0, 0);

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
    boot_options_view.setPage(BOOT_OPTIONS_HOME);
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

    update_boot_mode_button();
    update_regional_options_ui();
    update_face_customization_options_ui();
    update_clock_face_selection(true);
    update_screensaver_mode_button(true);
    update_screensaver_delay_selection(true);
    update_language_selection(true);
    update_night_options_ui();
    update_chime_options_ui();
    update_wifi_options_ui();
    boot_options_view.refreshUpdate();
    boot_options_view.setPage(
        boot_options_view.page_on_show);
    boot_options_view.page_on_show =
        BOOT_OPTIONS_HOME;

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
