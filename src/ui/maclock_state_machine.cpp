#ifdef MACLOCK_COMBINED_SOURCE
void MaclockApp::begin()
{
    active_app = this;
    hal_.begin();
    Serial.begin(115200);
    analogWrite(TFT_BL_VAR, 0);
    settings_store.begin();
    app_settings = settings_store.load();
    hal_.overrideBootEmulator(
        app_settings.boot_floppy_emulator);
    localization_set_language(app_settings.language);
    datetime_editor.setDateFormat(g_date_format);
    audio_service.begin();
    alarm_service.begin(settings_store.preferences());
    wifi_service.begin(settings_store.preferences());
    mqtt_service.begin(settings_store.preferences(), *this);
    if (hal_.isLocal())
        wifi_service.startTask();
    const String saved_chime_path = settings_store.loadChimePath(
        g_legacy_chime_sound_paths[g_chime.sound]);
    strlcpy(
        g_chime_sound_path, saved_chime_path.c_str(),
        sizeof(g_chime_sound_path));

    heap_caps_malloc_extmem_enable(0);
    LittleFS.begin();
    SoundSelector::scan();
    update_service.begin(settings_store.preferences());
    const String saved_startup_path =
        settings_store.loadStartupSoundPath("/startup.mp3");
    strlcpy(
        g_startup_sound_path, saved_startup_path.c_str(),
        sizeof(g_startup_sound_path));
    g_startup_sound_volume =
        settings_store.loadStartupSoundVolume(80);
    const String saved_floppy_path =
        settings_store.loadFloppySoundPath("/floppy.mp3");
    strlcpy(
        g_floppy_sound_path, saved_floppy_path.c_str(),
        sizeof(g_floppy_sound_path));
    g_floppy_sound_volume =
        settings_store.loadFloppySoundVolume(60);
    timer_service.begin(settings_store.preferences());
    control_panel_service.begin(*this);
    EmulatorHardwareBridge::bind(display_service);
    display_service.beginPanel();
    touch_eeprom_begin();
    input_service.begin();
    i2c_bus.begin(I2C_SDA, I2C_SCL);
    touchscreen_available_ = i2c_bus.present(0x38);
    touch_set_available(touchscreen_available_);
    Serial.println(
        touchscreen_available_
            ? "Touchscreen detected at 0x38"
            : "Touchscreen not detected; rotary navigation enabled");

    // LVGL renders a 304x224 viewport, not the full 320x240 panel.
    touch_init(
        display_service.tft().width() - 16,
        display_service.tft().height() - 16,
        display_service.tft().getRotation());
    if (touchscreen_available_)
        touch_load_calibration();

    apply_boot_brightness(g_boot_brightness, false);

    const bool boot_options_requested = !digitalRead(GPIO_CLOCK);
    hal_.appReady();
    bool emulator_returned_to_menu = false;

    if (!boot_options_requested && g_boot_floppy_emulator &&
        touchscreen_available_) {
        run_emulator();
        emulator_returned_to_menu = true;
    }

    display_service.beginCodec();
    SoundSelector::setPreviewCallback(audio_preview_play);
    display_service.beginLvgl();
    display_service.beginLvglInput();
    display_service.registerLittleFs();
    ui_shell.init();
    if (!touchscreen_available_)
    {
        if (boot_options_view.home_calibration_label)
            lv_obj_add_flag(
                boot_options_view.home_calibration_label,
                LV_OBJ_FLAG_HIDDEN);
        if (boot_options_view.calibration_label)
            lv_obj_add_flag(
                boot_options_view.calibration_label,
                LV_OBJ_FLAG_HIDDEN);
        if (boot_options_view.emulator_button_label)
        {
            lv_label_set_text(
                boot_options_view.emulator_button_label,
                tr("Touchscreen required"));
            lv_obj_add_state(
                lv_obj_get_parent(
                    boot_options_view.emulator_button_label),
                LV_STATE_DISABLED);
        }
        if (boot_options_view.boot_mode_button_label)
        {
            lv_label_set_text(
                boot_options_view.boot_mode_button_label,
                tr("Boot: Clock"));
            lv_obj_add_state(
                lv_obj_get_parent(
                    boot_options_view.boot_mode_button_label),
                LV_STATE_DISABLED);
        }
    }
    rtc_service.begin();
    {
        char rtc_status[64];
        format_rtc_health(rtc_status, sizeof(rtc_status));
        Serial.println(rtc_status);
    }

    pinMode(GPIO_CHARGING, INPUT_PULLDOWN);
    pinMode(GPIO_BAT_EN, OUTPUT);
    digitalWrite(GPIO_BAT_EN, 1);

    input_service.startTask();

    audio_service.startTask();
    weather_service.begin();

    if (boot_options_requested || emulator_returned_to_menu)
        request_state(UI_STATE_BOOT_OPTIONS);
}

void MaclockApp::tick()
{
    hal_.pump();
    unsigned long now = millis();
    InputSnapshot inputs = input_service.read();
    const bool screen_touch_pressed =
        touch_consume_press_edge();
    const int observed_encoder = input_service.encoderPosition();
    const bool rotary_activity =
        screensaver_last_encoder_ != INT32_MIN &&
        observed_encoder != screensaver_last_encoder_;
    screensaver_last_encoder_ = observed_encoder;
    timer_service.update(now);

    const auto is_rotary_menu = [](UiState state)
    {
        return state == UiState::SetDateTime ||
               state == UiState::BootOptions ||
               state == UiState::Diagnostics ||
               state == UiState::AlarmEditor ||
               state == UiState::AlarmRinging ||
               state == UiState::TimerEditor ||
               state == UiState::TimerFinished ||
               state == UiState::WifiSetup;
    };

    if (requested_state_ != UiState::None)
    {
        current_state_ = requested_state_;
        state_start_ms_ = now;
        requested_state_ = UiState::None;
    }
    if (current_state_ != UI_STATE_NORMAL)
        clock_view.hideCustomFace();

    if (last_state_ == UI_STATE_WIFI_SETUP &&
        current_state_ != UI_STATE_WIFI_SETUP)
    {
        wifi_service.stopPortal();
    }
    const WifiModeSnapshot wifi = wifi_service.snapshot();
    WifiModeSnapshot service_wifi = wifi;
    // Association and DHCP complete before the first forecast/NTP pass.
    // Keep other TCP/TLS clients idle until that pass has released the
    // Wi-Fi worker's network resources.
    service_wifi.connected =
        wifi.connected && strncmp(wifi.status, "Online", 6) == 0;

    const bool update_prompt_allowed =
        current_state_ == UiState::Normal &&
        !audio_service.running() &&
        !clock_view.screensaver_active;
    const bool update_page_active =
        current_state_ == UI_STATE_BOOT_OPTIONS &&
        boot_options_view.page == BOOT_OPTIONS_UPDATE &&
        !audio_service.running();
    const bool update_network_check_allowed =
        (update_prompt_allowed || update_page_active) &&
        !control_panel_service.backgroundNetworkActive();
    update_service.tick(
        service_wifi, update_prompt_allowed,
        update_network_check_allowed);
    const bool update_network_active =
        update_service.networkOperationActive() ||
        update_service.needsNetworkCheck(service_wifi);

    if (current_state_ == UI_STATE_WIFI_SETUP)
    {
        control_panel_service.stop();
        mqtt_service.stop(false);
    }
    else
    {
        WifiModeSnapshot control_wifi = service_wifi;
        if (update_network_active)
            control_wifi.connected = false;
        control_panel_service.tick(control_wifi);
        const bool mqtt_display_allowed =
            current_state_ != UI_STATE_ALARM_RINGING &&
            current_state_ != UI_STATE_TIMER_FINISHED &&
            current_state_ != UI_STATE_EMULATOR;
        WifiModeSnapshot mqtt_wifi = service_wifi;
        if (control_panel_service.backgroundNetworkActive() ||
            update_network_active)
            mqtt_wifi.connected = false;
        mqtt_service.tick(mqtt_wifi, mqtt_display_allowed, now);
    }
    if (mqtt_service.displayActive())
        full_brightness_until_ms_ = now + 2000;

    syncUpdatePrompt(update_prompt_allowed);

    if (control_preview_pending_ &&
        static_cast<int32_t>(
            now - control_preview_due_ms_) >= 0)
    {
        control_preview_pending_ = false;
        audio_service.play(
            control_preview_sound_,
            control_preview_volume_);
    }

    uint32_t synchronized_epoch = 0;
    if (wifi_service.takeTimeSync(synchronized_epoch))
    {
        app_events.adjustRtc(DateTime(synchronized_epoch));
        const DateTime synchronized = rtc_now();
        Serial.printf(
            "RTC synchronized by NTP: %04d-%02d-%02d %02d:%02d:%02d\n",
            synchronized.year(), synchronized.month(), synchronized.day(),
            synchronized.hour(), synchronized.minute(),
            synchronized.second());
    }

    if ((current_state_ == UI_STATE_NORMAL ||
         current_state_ == UI_STATE_SET_DATETIME ||
         current_state_ == UI_STATE_ALARM_EDITOR ||
         current_state_ == UI_STATE_TIMER_EDITOR) &&
        (!last_alarm_check_ms_ || now - last_alarm_check_ms_ >= 250))
    {
        last_alarm_check_ms_ = now;
        const int due_alarm = alarm_service.due(rtc_now());
        if (due_alarm >= 0)
        {
            active_alarm_index_ = due_alarm;
            current_state_ = UI_STATE_ALARM_RINGING;
            state_start_ms_ = now;
        }
    }

    if (current_state_ != UI_STATE_ALARM_RINGING &&
        current_state_ != UI_STATE_TIMER_FINISHED &&
        timer_service.takeFinished())
    {
        current_state_ = UI_STATE_TIMER_FINISHED;
        state_start_ms_ = now;
    }

    if (!last_night_check_ms_ || now - last_night_check_ms_ >= 1000)
    {
        const DateTime current = rtc_now();
        scheduled_display_state_ = night_display_state(current);
        if (current_state_ == UI_STATE_NORMAL ||
            current_state_ == UI_STATE_SET_DATETIME ||
            current_state_ == UI_STATE_ALARM_EDITOR ||
            current_state_ == UI_STATE_TIMER_EDITOR)
        {
            maybe_start_chime(current);
        }
        last_night_check_ms_ = now;
    }

    const bool temporary_wake_active =
        (int32_t)(full_brightness_until_ms_ - now) > 0;
    if (current_state_ == UI_STATE_NORMAL &&
        !clock_view.screensaver_active &&
        scheduled_display_state_ != NIGHT_DISPLAY_NORMAL &&
        !temporary_wake_active &&
        (inputs.clock || inputs.alarm))
    {
        full_brightness_until_ms_ = now + 10000;
        inputs.clock = false;
        inputs.alarm = false;
    }

    switch (current_state_)
    {
    case UiState::None:
        break;
    case UI_STATE_EMPTY_SCREEN: //  empty screen, start sound
        if (now - state_start_ms_ >= 0)
        {
            ui_shell.hideAll();
            lv_obj_clear_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
            lv_timer_handler();
            startup_sound_started_ = audio_service.play(
                SoundSelector::resolvePath(
                    g_startup_sound_path, "/startup.mp3"),
                g_startup_sound_volume);
            requested_state_ = advance_state(current_state_);
            state_start_ms_ = now;
        }
        break;
    case UI_STATE_WAIT_STARTUP_SOUND: // wait for end of startup sound
    {
        static constexpr uint32_t kStartupSoundTimeoutMs = 15000;
        if (!startup_sound_started_ ||
            audio_service.takeFinished() ||
            now - state_start_ms_ >= kStartupSoundTimeoutMs)
        {
            startup_sound_started_ = false;
            wifi_service.startTask();
            requested_state_ = advance_state(current_state_);
            state_start_ms_ = now;
        }
    }
    break;
    case UI_STATE_WAIT_FLOPPY_1: // wait for floppy 1
        if (now - state_start_ms_ >= 1000)
        {
            ui_shell.hideAll();
            lv_obj_clear_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.disk_missing_1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
            lv_timer_handler();
            requested_state_ = advance_state(current_state_);
            state_start_ms_ = now;
        }
        if (inputs.floppy)
        {
            current_state_ = advance_state(current_state_, 2);
            state_start_ms_ = now;
        }
        break;
    case UI_STATE_WAIT_FLOPPY_2: // wait for floppy 2
        if (now - state_start_ms_ >= 1000)
        {
            ui_shell.hideAll();
            lv_obj_clear_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.disk_missing_2, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
            lv_timer_handler();

            current_state_ = static_cast<UiState>(
                static_cast<uint8_t>(current_state_) - 1);
            state_start_ms_ = now;
        }
        if (inputs.floppy)
        {
            requested_state_ = advance_state(current_state_);
            state_start_ms_ = now;
        }
        break;
    case UI_STATE_FLOPPY_INSERTED: // floppy inserted, loading...
    {
        ui_shell.hideAll();
        lv_obj_clear_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
        lv_timer_handler();
        floppy_sound_started_ = false;
    }
        requested_state_ = advance_state(current_state_);
        state_start_ms_ = now;
        break;
    case UI_STATE_BOOT_PLUGINS: // show boot screen + detected i2c plugins
        if (current_state_ != last_state_)
        {
            const uint8_t k_addrs[k_plugin_max] = {
                0x18, 0x38, weather_service.address(), 0x68};
            const int16_t margin_x = 8;
            const int16_t margin_y = 8;
            const int16_t spacing = 4;
            const int16_t icon_size = 32;
            startup_view.plugin_count = 0;
            startup_view.plugin_reveal = 0;
            startup_view.next_reveal_ms = now + (unsigned long)random(100, 301);
            for (size_t i = 0; i < k_plugin_max; ++i)
            {
                // Remove any previous RAM-backed source before releasing it.
                set_image_src(
                    ui_shell.plugin_icons[i], ui_shell.plugin_buf,
                    "S:/plugin.png");
                if (startup_view.plugin_buffers[i])
                {
                    lv_draw_buf_destroy(startup_view.plugin_buffers[i]);
                    startup_view.plugin_buffers[i] = nullptr;
                }
            }
            for (size_t i = 0; i < k_plugin_max; ++i)
            {
                const uint8_t addr = k_addrs[i];
                const size_t slot = startup_view.plugin_count;
                if (addr != 0 && i2c_bus.present(addr) && startup_view.plugin_count < k_plugin_max)
                {
                    char fs_path[32];
                    char lv_path[36];
                    snprintf(fs_path, sizeof(fs_path), "/plugin_0x%02X.png", addr);
                    snprintf(lv_path, sizeof(lv_path), "S:/plugin_0x%02X.png", addr);
                    if (littlefs_exists(fs_path))
                    {
                        startup_view.plugin_buffers[slot] =
                            load_png_once(lv_path);
                        set_image_src(
                            ui_shell.plugin_icons[slot],
                            startup_view.plugin_buffers[slot]
                                ? startup_view.plugin_buffers[slot]
                                : ui_shell.plugin_buf,
                            "S:/plugin.png");
                    }
                    else
                        set_image_src(ui_shell.plugin_icons[slot], ui_shell.plugin_buf, "S:/plugin.png");
                    lv_obj_align(ui_shell.plugin_icons[startup_view.plugin_count], LV_ALIGN_BOTTOM_LEFT,
                                 margin_x + (int16_t)startup_view.plugin_count * (icon_size + spacing),
                                 -margin_y);
                    startup_view.plugin_count++;
                }
                else
                {
                    char fs_path[32];
                    char lv_path[36];
                    snprintf(fs_path, sizeof(fs_path), "/plugin_0x%02X.png", addr);
                    snprintf(lv_path, sizeof(lv_path), "S:/plugin_0x%02X.png", addr);
                    if (littlefs_exists(fs_path))
                    {
                        lv_draw_buf_t *loaded = load_png_once(lv_path);
                        startup_view.plugin_buffers[slot] =
                            make_plugin_missing_buf(loaded);
                        if (loaded)
                            lv_draw_buf_destroy(loaded);
                        set_image_src(
                            ui_shell.plugin_icons[slot],
                            startup_view.plugin_buffers[slot]
                                ? startup_view.plugin_buffers[slot]
                                : ui_shell.plugin_missing_buf,
                            "S:/plugin.png");
                    }
                    else
                    {
                        set_image_src(ui_shell.plugin_icons[startup_view.plugin_count], ui_shell.plugin_missing_buf, "S:/plugin.png");
                    }
                    lv_obj_align(ui_shell.plugin_icons[startup_view.plugin_count], LV_ALIGN_BOTTOM_LEFT,
                                 margin_x + (int16_t)startup_view.plugin_count * (icon_size + spacing),
                                 -margin_y);
                    startup_view.plugin_count++;
                }
            }
            for (size_t i = startup_view.plugin_count; i < k_plugin_max; ++i)
                lv_obj_add_flag(ui_shell.plugin_icons[i], LV_OBJ_FLAG_HIDDEN);

            // Every image source is now RAM-backed. Audio can stream from
            // LittleFS while the icons are revealed without competing with
            // LVGL filesystem reads.
            floppy_sound_started_ = audio_service.play(
                SoundSelector::resolvePath(
                    g_floppy_sound_path, "/floppy.mp3"),
                g_floppy_sound_volume, true);
        }
        if (now - state_start_ms_ >= 0)
        {
            ui_shell.hideAll();
            lv_obj_clear_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.boot, LV_OBJ_FLAG_HIDDEN);
            if (startup_view.plugin_reveal < startup_view.plugin_count && now >= startup_view.next_reveal_ms)
            {
                startup_view.plugin_reveal++;
                startup_view.next_reveal_ms = now + (unsigned long)random(200, 600);
            }
            for (size_t i = 0; i < startup_view.plugin_reveal; ++i)
                lv_obj_clear_flag(ui_shell.plugin_icons[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
            lv_timer_handler();
        }
        if (now - state_start_ms_ >= 1500 && startup_view.plugin_reveal == startup_view.plugin_count)
        {
            requested_state_ = advance_state(current_state_);
            state_start_ms_ = now;
        }
        break;
    case UI_STATE_WAIT_FLOPPY_SOUND: // wait for end of floppy sound
    {
        static constexpr uint32_t kFloppySoundTimeoutMs = 15000;
        const bool sound_finished =
            audio_service.takeFinished();
        if (!floppy_sound_started_ || sound_finished ||
            now - state_start_ms_ >= kFloppySoundTimeoutMs)
        {
            floppy_sound_started_ = false;
            requested_state_ = advance_state(current_state_);
            state_start_ms_ = now;
        }
    }
    break;
    case UI_STATE_NORMAL: // normal state
    {
        static constexpr unsigned long kDualKeyHoldMs = 2000;
        if (current_state_ != last_state_)
        {
            wifi_service.startTask();
            clock_view.last_update_ms = 0;
            dual_key_hold_start_ms_ = 0;
            dual_key_handled_ = false;
            clock_press_pending_ = false;
            alarm_press_pending_ = false;
            clock_view.last_activity_ms = now;
            screensaver_last_encoder_ = observed_encoder;
            const ClockRenderSnapshot snapshot =
                make_clock_snapshot(now);
            clock_view.show(snapshot);
            clock_view.update(snapshot);
            lv_timer_handler();
        }

        if (screensaver_launch_pending_ &&
            screensaver_preview_mode_ != ScreensaverMode::Off)
        {
            screensaver_launch_pending_ = false;
            clock_view.last_activity_ms = now;
            clock_view.showScreensaver(screensaver_preview_mode_);
            screensaver_preview_mode_ = ScreensaverMode::Off;
            clock_view.updateScreensaver(
                make_clock_snapshot(now));
            lv_timer_handler();
            break;
        }

        const bool clock_activity =
            inputs.clock || inputs.alarm || inputs.touch ||
            screen_touch_pressed;
        if (clock_view.screensaver_active)
        {
            if (clock_activity)
            {
                clock_view.last_activity_ms = now;
                full_brightness_until_ms_ = now + 10000;
                inputs.clock = false;
                inputs.alarm = false;
                inputs.touch = false;
                dual_key_hold_start_ms_ = 0;
                dual_key_handled_ = false;
                clock_press_pending_ = false;
                alarm_press_pending_ = false;
                const ClockRenderSnapshot snapshot =
                    make_clock_snapshot(now);
                clock_view.show(snapshot);
                clock_view.update(snapshot);
            }
            else
            {
                if (!clock_view.screensaver_snapshot_ms ||
                    now - clock_view.screensaver_snapshot_ms >= 250)
                {
                    clock_view.screensaver_snapshot.current = rtc_now();
                    clock_view.screensaver_snapshot_ms = now;
                }
                clock_view.updateScreensaver(
                    clock_view.screensaver_snapshot);
            }
            lv_timer_handler();
            break;
        }

        if (clock_activity)
            clock_view.last_activity_ms = now;
        const unsigned long screensaver_delay_ms =
            (unsigned long)
                g_screensaver_delays_minutes[
                    g_screensaver_delay_index] *
            60000UL;
        if (g_screensaver_mode != SCREENSAVER_OFF &&
            now - clock_view.last_activity_ms >=
                screensaver_delay_ms)
        {
            clock_view.showScreensaver(g_screensaver_mode);
            clock_view.screensaver_snapshot.current = rtc_now();
            clock_view.screensaver_snapshot_ms = now;
            clock_view.updateScreensaver(
                clock_view.screensaver_snapshot);
            lv_timer_handler();
            break;
        }

        if (!clock_view.last_update_ms || now - clock_view.last_update_ms >= 100)
        {
            if (ui_shell.icon)
                lv_obj_add_flag(ui_shell.icon, LV_OBJ_FLAG_HIDDEN);
            clock_view.update(make_clock_snapshot(now));
            lv_timer_handler();
            clock_view.last_update_ms = now;
        }

        const bool clock_button_down = digitalRead(GPIO_CLOCK) == LOW;
        const bool alarm_button_down = digitalRead(GPIO_ALARM) == LOW;
        if (clock_button_down && alarm_button_down)
        {
            if (!dual_key_hold_start_ms_)
                dual_key_hold_start_ms_ = now;
            else if (!dual_key_handled_ &&
                     now - dual_key_hold_start_ms_ >= kDualKeyHoldMs)
            {
                dual_key_handled_ = true;
                clock_press_pending_ = false;
                alarm_press_pending_ = false;
                requested_state_ = UI_STATE_BOOT_OPTIONS;
                state_start_ms_ = now;
            }
        }
        else
        {
            dual_key_hold_start_ms_ = 0;
            if (!clock_button_down && !alarm_button_down)
                dual_key_handled_ = false;
        }

        if (inputs.clock && !dual_key_handled_)
            clock_press_pending_ = true;
        if (inputs.alarm && !dual_key_handled_)
            alarm_press_pending_ = true;

        if (dual_key_handled_)
        {
            clock_press_pending_ = false;
            alarm_press_pending_ = false;
        }
        else if (clock_press_pending_ && !clock_button_down)
        {
            clock_press_pending_ = false;
            alarm_press_pending_ = false;
            requested_state_ = UI_STATE_BOOT_OPTIONS;
            state_start_ms_ = now;
        }
        else if (alarm_press_pending_ && !alarm_button_down)
        {
            clock_press_pending_ = false;
            alarm_press_pending_ = false;
            requested_state_ = UI_STATE_ALARM_EDITOR;
            state_start_ms_ = now;
        }
        break;
    }
    case UI_STATE_SET_DATETIME: // change date/time
        if (current_state_ != last_state_)
        {
            DateTime current = rtc_now();
            datetime_editor.enter(current);
        }
        if (now - state_start_ms_ >= 0)
        {
            ui_shell.hideAll();
            lv_obj_clear_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.white_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.black_line, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
            datetime_editor.show();
            lv_timer_handler();
        }
        break;
    case UI_STATE_ALARM_EDITOR:
        if (current_state_ != last_state_)
            alarm_view.enter(rtc_now());
        ui_shell.hideAll();
        lv_obj_clear_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.white_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.black_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
        alarm_view.showEditor();
        lv_timer_handler();
        break;
    case UI_STATE_ALARM_RINGING:
        if (current_state_ != last_state_)
        {
            ui_shell.hideAll();
            lv_obj_clear_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.white_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.black_line, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
            alarm_view.showRinging((size_t)active_alarm_index_);
            active_alarm_volume_ =
                alarm_service.ringingVolume(
                    (size_t)active_alarm_index_, 0);
            audio_service.play(
                alarm_service.soundPath((size_t)active_alarm_index_),
                active_alarm_volume_);
        }
        if (active_alarm_index_ >= 0)
        {
            const uint32_t alarm_elapsed =
                now - state_start_ms_;
            alarm_view.updateRinging(
                (size_t)active_alarm_index_,
                alarm_elapsed);
            const uint8_t ramp_volume =
                alarm_service.ringingVolume(
                    (size_t)active_alarm_index_,
                    alarm_elapsed);
            if (ramp_volume != active_alarm_volume_)
            {
                active_alarm_volume_ = ramp_volume;
                audio_service.setVolume(
                    active_alarm_volume_);
            }
        }
        lv_timer_handler();
        if (inputs.alarm || inputs.touch)
            app_events.snoozeActiveAlarm();
        else if (inputs.clock)
            app_events.dismissActiveAlarm();
        else if (audio_service.takeFinished() && active_alarm_index_ >= 0)
        {
            audio_service.play(
                alarm_service.soundPath((size_t)active_alarm_index_),
                active_alarm_volume_);
        }
        break;
    case UI_STATE_TIMER_EDITOR:
        if (current_state_ != last_state_)
            timer_view.enter(now);
        ui_shell.hideAll();
        lv_obj_clear_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.white_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.black_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
        timer_view.show(now);
        lv_timer_handler();
        break;
    case UI_STATE_TIMER_FINISHED:
        if (current_state_ != last_state_)
        {
            ui_shell.hideAll();
            lv_obj_clear_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.white_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.black_line, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
            timer_view.showFinished();
            audio_service.play(timer_service.soundPath(), timer_service.volume());
        }
        lv_timer_handler();
        if (inputs.clock || inputs.alarm)
            app_events.dismissTimer();
        else if (audio_service.takeFinished())
            audio_service.play(timer_service.soundPath(), timer_service.volume());
        break;
    case UI_STATE_BOOT_OPTIONS:
        if (current_state_ != last_state_)
        {
            boot_options_view.clock_armed = false;
            ui_shell.hideAll();
            lv_obj_clear_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
            boot_options_view.show();
        }
        boot_options_view.tick(now);
        lv_timer_handler();
        if (!boot_options_view.clock_armed)
        {
            if (digitalRead(GPIO_CLOCK))
                boot_options_view.clock_armed = true;
        }
        else if (inputs.clock)
        {
            if (touchscreen_available_)
            {
                requested_state_ = UI_STATE_CALIBRATION;
                state_start_ms_ = now;
            }
        }
        break;
    case UI_STATE_EMULATOR:
        if (touchscreen_available_)
            run_emulator();
        requested_state_ = UI_STATE_BOOT_OPTIONS;
        state_start_ms_ = now;
        break;
    case UI_STATE_DIAGNOSTICS:
    {
        if (current_state_ != last_state_)
        {
            ui_shell.hideAll();
            lv_obj_clear_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(diagnostics_view.panel, LV_OBJ_FLAG_HIDDEN);
            diagnostics_view.last_update_ms = 0;
        }
        if (!diagnostics_view.last_update_ms ||
            now - diagnostics_view.last_update_ms >= 250)
        {
            diagnostics_view.update(
                make_diagnostics_snapshot());
            lv_timer_handler();
            diagnostics_view.last_update_ms = now;
        }
        break;
    }
    case UI_STATE_WIFI_SETUP:
    {
        if (current_state_ != last_state_)
        {
            ui_shell.hideAll();
            lv_obj_clear_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(
                wifi_setup_view.panel, LV_OBJ_FLAG_HIDDEN);
            control_panel_service.stop();
            wifi_service.startPortal();
            wifi_setup_view.last_update_ms = 0;
        }
        wifi_service.processPortal();
        if (!wifi_setup_view.last_update_ms ||
            now - wifi_setup_view.last_update_ms >= 500)
        {
            const WifiModeSnapshot wifi = wifi_service.snapshot();
            update_wifi_setup_status_label(
                wifi.status);
            lv_timer_handler();
            wifi_setup_view.last_update_ms = now;
        }
        break;
    }
    case UI_STATE_CALIBRATION: // calibration screen
        if (inputs.clock)
        {
            requested_state_ = UI_STATE_BOOT_OPTIONS;
            state_start_ms_ = now;
            break;
        }
        if (current_state_ != last_state_)
        {
            lv_obj_t *scr = lv_screen_active();
            int w = lv_obj_get_width(scr);
            int h = lv_obj_get_height(scr);
            int margin = 16;
            calibration_view.targets[0] = {margin, margin};
            calibration_view.targets[1] = {w - 1 - margin, margin};
            calibration_view.targets[2] = {w - 1 - margin, h - 1 - margin};
            calibration_view.targets[3] = {margin, h - 1 - margin};
            calibration_view.step = 0;
            calibration_view.wait_release = false;
            calibration_view.sum_x = 0;
            calibration_view.sum_y = 0;
            calibration_view.sample_count = 0;
            if (g_cursor)
                lv_obj_add_flag(g_cursor, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(calibration_view.label, tr("Touch the crosshair"));
            calib_set_cross_pos(calibration_view.targets[0]);
        }
        if (now - state_start_ms_ >= 0)
        {
            ui_shell.hideAll();
            lv_obj_clear_flag(ui_shell.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(calibration_view.label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(calibration_view.cross, LV_OBJ_FLAG_HIDDEN);
            lv_timer_handler();
        }
        {
            uint16_t raw_x = 0;
            uint16_t raw_y = 0;
            bool touched = touch_read_raw(raw_x, raw_y);
            if (touched && !calibration_view.wait_release)
            {
                calibration_view.wait_release = true;
                calibration_view.sum_x = 0;
                calibration_view.sum_y = 0;
                calibration_view.sample_count = 0;
            }
            if (touched && calibration_view.wait_release && calibration_view.sample_count < 64)
            {
                calibration_view.sum_x += raw_x;
                calibration_view.sum_y += raw_y;
                calibration_view.sample_count++;
            }
            if (!touched && calibration_view.wait_release)
            {
                calibration_view.wait_release = false;
                if (calibration_view.sample_count == 0)
                    break;
                calibration_view.raw_x[calibration_view.step] =
                    (calibration_view.sum_x + calibration_view.sample_count / 2) /
                    calibration_view.sample_count;
                calibration_view.raw_y[calibration_view.step] =
                    (calibration_view.sum_y + calibration_view.sample_count / 2) /
                    calibration_view.sample_count;
                calibration_view.step++;
                if (calibration_view.step < 4)
                {
                    calib_set_cross_pos(calibration_view.targets[calibration_view.step]);
                }
                else
                {
                    lv_obj_t *scr = lv_screen_active();
                    int w = lv_obj_get_width(scr);
                    int h = lv_obj_get_height(scr);
                    uint16_t minx = 0;
                    uint16_t maxx = 0;
                    uint16_t miny = 0;
                    uint16_t maxy = 0;
                    bool valid =
                        calib_axis_bounds(
                            calibration_view.raw_x[0], calibration_view.raw_x[3],
                            calibration_view.raw_x[1], calibration_view.raw_x[2],
                            calibration_view.targets[0].x, calibration_view.targets[1].x,
                            w, minx, maxx) &&
                        calib_axis_bounds(
                            calibration_view.raw_y[0], calibration_view.raw_y[1],
                            calibration_view.raw_y[3], calibration_view.raw_y[2],
                            calibration_view.targets[0].y, calibration_view.targets[3].y,
                            h, miny, maxy);
                    if (valid)
                    {
                        touch_set_calibration(minx, maxx, miny, maxy);
                        touch_save_calibration();
                        requested_state_ = UI_STATE_NORMAL;
                        state_start_ms_ = now;
                    }
                    else
                    {
                        calibration_view.step = 0;
                        lv_label_set_text(
                            calibration_view.label,
                            tr("Calibration failed - try again"));
                        calib_set_cross_pos(calibration_view.targets[0]);
                    }
                }
            }
        }
        break;
    }

    last_state_ = current_state_;

    const bool rotary_menu =
        !touchscreen_available_ && is_rotary_menu(current_state_);
    if (rotary_menu != rotary_menu_active_)
    {
        if (rotary_menu)
        {
            rotary_brightness_position_ =
                observed_encoder < 0
                    ? 0
                    : (observed_encoder > kBrightnessMax
                           ? kBrightnessMax
                           : observed_encoder);
            rotary_last_position_ = observed_encoder;
            rotary_navigator_.enter();
        }
        else
        {
            rotary_navigator_.leave();
            input_service.setEncoderPosition(
                rotary_brightness_position_);
            screensaver_last_encoder_ =
                rotary_brightness_position_;
        }
        rotary_menu_active_ = rotary_menu;
    }
    if (rotary_menu_active_)
    {
        const int current_encoder =
            input_service.encoderPosition();
        const int delta = current_encoder - rotary_last_position_;
        if (delta)
        {
            rotary_navigator_.move(delta > 0 ? 1 : -1);
            rotary_last_position_ = current_encoder;
        }
        if (inputs.touch)
            rotary_navigator_.activate();
    }

    if (inputs.touch || inputs.clock || inputs.alarm ||
        screen_touch_pressed) {
        full_brightness_until_ms_ = now + 10000;
    } else if (rotary_activity) {
        full_brightness_until_ms_ = now;
    }

    int enc = rotary_menu_active_
                  ? rotary_brightness_position_
                  : input_service.encoderPosition();
    if (enc < 0)
        enc = 0;
    if (enc > kBrightnessMax)
        enc = kBrightnessMax;
    if (!rotary_menu_active_ &&
        enc != input_service.encoderPosition())
        input_service.setEncoderPosition(enc);
    if (current_state_ == UI_STATE_ALARM_RINGING &&
        active_alarm_index_ >= 0 &&
        alarm_service.settings(
            (size_t)active_alarm_index_).sunrise)
    {
        static constexpr uint32_t kSunriseDurationMs = 60000;
        const uint32_t elapsed = now - state_start_ms_;
        const uint32_t clamped =
            elapsed < kSunriseDurationMs
                ? elapsed
                : kSunriseDurationMs;
        const int sunrise_pwm =
            16 + static_cast<int>(
                     (239ULL * clamped) /
                     kSunriseDurationMs);
        analogWrite(TFT_BL_VAR, sunrise_pwm);
    }
    const int32_t wake_remaining = (int32_t)(full_brightness_until_ms_ - now);
    if (wake_remaining > 0) {
        analogWrite(TFT_BL_VAR, 255);
    } else if (current_state_ == UI_STATE_NORMAL &&
               scheduled_display_state_ == NIGHT_DISPLAY_OFF) {
        analogWrite(TFT_BL_VAR, 0);
    } else if (current_state_ == UI_STATE_NORMAL &&
               scheduled_display_state_ == NIGHT_DISPLAY_DIMMED) {
        analogWrite(TFT_BL_VAR, brightness_to_pwm(min(enc, 1)));
    } else {
        analogWrite(TFT_BL_VAR, brightness_to_pwm(enc));
    }

    if (enc != g_last_saved_encoder && (now - last_encoder_save_ms_) >= 500)
    {
        settings_store.saveBrightness((uint8_t)enc);
        g_last_saved_encoder = enc;
        last_encoder_save_ms_ = now;
    }
}

#endif
