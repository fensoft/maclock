#ifdef MACLOCK_COMBINED_SOURCE

ControlPanelSnapshot MaclockApp::controlPanelSnapshot()
{
    ControlPanelSnapshot snapshot;
    snapshot.settings = app_settings;
    snapshot.screensaver_active =
        clock_view.screensaver_active;

    int brightness = input_service.encoderPosition();
    if (brightness < 0)
        brightness = 0;
    if (brightness > kBrightnessMax)
        brightness = kBrightnessMax;
    snapshot.brightness = static_cast<uint8_t>(brightness);
    strlcpy(
        snapshot.startup_sound,
        SoundSelector::resolvePath(
            g_startup_sound_path, "/startup.mp3"),
        sizeof(snapshot.startup_sound));
    snapshot.startup_volume = g_startup_sound_volume;
    strlcpy(
        snapshot.floppy_sound,
        SoundSelector::resolvePath(
            g_floppy_sound_path, "/floppy.mp3"),
        sizeof(snapshot.floppy_sound));
    snapshot.floppy_volume = g_floppy_sound_volume;
    strlcpy(
        snapshot.chime_sound,
        SoundSelector::resolvePath(
            g_chime_sound_path, "/quack.mp3"),
        sizeof(snapshot.chime_sound));

    for (size_t i = 0;
         i < kControlPanelAlarmCount && i < kAlarmCount;
         ++i)
    {
        const AlarmSettings alarm =
            alarm_service.settings(i);
        snapshot.alarms[i].enabled = alarm.enabled;
        snapshot.alarms[i].hour = alarm.hour;
        snapshot.alarms[i].minute = alarm.minute;
        snapshot.alarms[i].weekdays = alarm.weekdays;
        snapshot.alarms[i].volume = alarm.volume;
        snapshot.alarms[i].one_time =
            alarm.one_time != 0;
        snapshot.alarms[i].gradual_volume =
            alarm.gradual_volume != 0;
        snapshot.alarms[i].sunrise =
            alarm.sunrise != 0;
        strlcpy(
            snapshot.alarms[i].label,
            alarm.label,
            sizeof(snapshot.alarms[i].label));
        strlcpy(
            snapshot.alarms[i].sound,
            alarm_service.soundPath(i),
            sizeof(snapshot.alarms[i].sound));
    }
    UpcomingAlarm upcoming;
    if (alarm_service.upcoming(rtc_now(), upcoming))
    {
        snapshot.upcoming_alarm.valid = true;
        snapshot.upcoming_alarm.snoozed =
            upcoming.snoozed;
        snapshot.upcoming_alarm.one_time =
            upcoming.one_time;
        snapshot.upcoming_alarm.index =
            static_cast<uint8_t>(upcoming.index);
        snapshot.upcoming_alarm.day_offset =
            upcoming.day_offset;
        snapshot.upcoming_alarm.weekday =
            upcoming.weekday;
        snapshot.upcoming_alarm.hour = upcoming.hour;
        snapshot.upcoming_alarm.minute = upcoming.minute;
        strlcpy(
            snapshot.upcoming_alarm.label,
            upcoming.label,
            sizeof(snapshot.upcoming_alarm.label));
    }

    snapshot.timer.active = timer_service.active();
    snapshot.timer.minutes =
        timer_service.selectedMinutes();
    snapshot.timer.remaining_seconds =
        timer_service.remainingSeconds(millis());
    snapshot.timer.volume =
        timer_service.volumeIndex();
    strlcpy(
        snapshot.timer.sound,
        timer_service.soundPath(),
        sizeof(snapshot.timer.sound));
    const WifiModeSnapshot wifi = wifi_service.snapshot();
    strlcpy(
        snapshot.location.city,
        wifi.city,
        sizeof(snapshot.location.city));
    strlcpy(
        snapshot.location.country,
        wifi.country,
        sizeof(snapshot.location.country));
    strlcpy(
        snapshot.location.resolved,
        wifi.location,
        sizeof(snapshot.location.resolved));
    strlcpy(
        snapshot.location.timezone,
        wifi.timezone,
        sizeof(snapshot.location.timezone));
    snapshot.update = update_service.snapshot();
    snapshot.mqtt = mqtt_service.snapshot();
    return snapshot;
}

ControlPanelConfiguration
MaclockApp::controlPanelConfiguration()
{
    const ControlPanelSnapshot snapshot =
        controlPanelSnapshot();
    ControlPanelConfiguration configuration;
    configuration.settings = snapshot.settings;
    configuration.brightness = snapshot.brightness;
    strlcpy(
        configuration.startup_sound,
        snapshot.startup_sound,
        sizeof(configuration.startup_sound));
    configuration.startup_volume =
        snapshot.startup_volume;
    strlcpy(
        configuration.floppy_sound,
        snapshot.floppy_sound,
        sizeof(configuration.floppy_sound));
    configuration.floppy_volume =
        snapshot.floppy_volume;
    strlcpy(
        configuration.chime_sound,
        snapshot.chime_sound,
        sizeof(configuration.chime_sound));
    memcpy(
        configuration.alarms, snapshot.alarms,
        sizeof(configuration.alarms));
    configuration.timer = snapshot.timer;
    configuration.timer.active = false;
    configuration.timer.remaining_seconds = 0;
    configuration.wifi = wifi_service.backupSettings();
    configuration.touch = touch_calibration();
    return configuration;
}

bool MaclockApp::applyControlConfiguration(
    const ControlPanelConfiguration &configuration,
    bool &network_changed)
{
    network_changed = false;
    if (configuration.settings.language >=
            UI_LANGUAGE_COUNT ||
        configuration.settings.date_format >=
            UI_DATE_FORMAT_COUNT ||
        configuration.settings.temperature_unit >=
            UI_TEMPERATURE_UNIT_COUNT ||
        static_cast<uint8_t>(
            configuration.settings.boot_brightness) >
            static_cast<uint8_t>(
                BootBrightness::Highest) ||
        configuration.brightness > kBrightnessMax)
    {
        return false;
    }

    if (!applyControlAppearance(
            configuration.settings.language,
            configuration.settings.clock_face,
            configuration.settings.clock_theme,
            configuration.brightness,
            configuration.settings.face_customization,
            configuration.settings.time_format) ||
        !applyControlScreensaver(
            configuration.settings.screensaver_mode,
            configuration.settings.screensaver_delay_index,
            false) ||
        !applyControlNightMode(
            configuration.settings.night_mode) ||
        !applyControlChime(
            configuration.settings.chime,
            configuration.chime_sound) ||
        !applyControlSystemSounds(
            configuration.startup_sound,
            configuration.startup_volume,
            configuration.floppy_sound,
            configuration.floppy_volume) ||
        !timer_service.configure(
            configuration.timer.minutes,
            configuration.timer.sound,
            configuration.timer.volume))
    {
        return false;
    }

    app_settings.date_format =
        configuration.settings.date_format;
    app_settings.temperature_unit =
        configuration.settings.temperature_unit;
    app_settings.boot_brightness =
        configuration.settings.boot_brightness;
    app_settings.boot_floppy_emulator =
        configuration.settings.boot_floppy_emulator;
    settings_store.saveDateFormat(
        app_settings.date_format);
    settings_store.saveTemperatureUnit(
        app_settings.temperature_unit);
    settings_store.saveBootBrightness(
        app_settings.boot_brightness);
    settings_store.saveBootMode(
        app_settings.boot_floppy_emulator);
    datetime_editor.setDateFormat(app_settings.date_format);
    set_checked_button(
        boot_options_view.brightness_options,
        static_cast<uint8_t>(
            app_settings.boot_brightness));
    update_regional_options_ui();

    alarm_service.dismiss();
    for (size_t index = 0;
         index < kControlPanelAlarmCount; ++index)
    {
        if (!applyControlAlarm(
                index, configuration.alarms[index]))
        {
            return false;
        }
    }

    if (!touch_restore_calibration(configuration.touch) ||
        !wifi_service.restoreSettings(
            configuration.wifi, network_changed))
    {
        return false;
    }

    update_wifi_options_ui();
    if (current_state_ == UiState::AlarmEditor)
    {
        alarm_view.enter(rtc_now());
        alarm_view.showEditor();
    }
    if (current_state_ == UiState::TimerEditor)
    {
        timer_view.enter(millis());
        timer_view.show(millis());
    }
    lv_timer_handler();
    return true;
}

bool MaclockApp::applyControlAppearance(
    UiLanguage language,
    ClockFace face, ClockTheme theme, uint8_t brightness,
    const FaceCustomizationSettings &face_customization,
    const TimeFormatSettings &time_format)
{
    if (language >= UI_LANGUAGE_COUNT ||
        static_cast<uint8_t>(face) >=
            static_cast<uint8_t>(ClockFace::Count) ||
        static_cast<uint8_t>(theme) >=
            static_cast<uint8_t>(ClockTheme::Count) ||
        static_cast<uint8_t>(time_format.hour_format) >=
            static_cast<uint8_t>(HourFormat::Count) ||
        static_cast<uint8_t>(face_customization.accent) >=
            static_cast<uint8_t>(FaceAccent::Count) ||
        static_cast<uint8_t>(
            face_customization.numeral_size) >=
            static_cast<uint8_t>(FaceNumeralSize::Count) ||
        static_cast<uint8_t>(face_customization.flip_speed) >=
            static_cast<uint8_t>(
                FlipAnimationSpeed::Count) ||
        brightness > kBrightnessMax)
    {
        return false;
    }

    const bool language_changed =
        app_settings.language != language;
    app_settings.language = language;
    app_settings.clock_face = face;
    app_settings.clock_theme = theme;
    app_settings.face_customization = face_customization;
    app_settings.time_format = time_format;
    settings_store.saveLanguage(language);
    settings_store.saveClockFace(face);
    settings_store.saveClockTheme(theme);
    settings_store.saveFaceCustomization(face_customization);
    settings_store.saveTimeFormat(time_format);
    settings_store.saveBrightness(brightness);
    input_service.setEncoderPosition(brightness);
    g_last_saved_encoder = brightness;
    last_encoder_save_ms_ = millis();
    set_checked_button(
        boot_options_view.clock_face_options,
        static_cast<uint8_t>(face));
    update_regional_options_ui();
    update_display_options_ui();
    if (language_changed)
    {
        localization_set_language(language);
        refresh_language_ui();
    }

    clock_view.applyTimeFormatLayout();
    clock_view.applyTheme();
    if (current_state_ == UiState::Normal)
    {
        clock_view.last_second = -1;
        const ClockRenderSnapshot snapshot =
            make_clock_snapshot(millis());
        clock_view.show(snapshot);
        clock_view.update(snapshot);
    }
    return true;
}

bool MaclockApp::applyControlScreensaver(
    ScreensaverMode mode, uint8_t delay_index,
    bool launch_now)
{
    if (static_cast<uint8_t>(mode) >=
            static_cast<uint8_t>(
                ScreensaverMode::Count) ||
        delay_index >= kScreensaverDelayCount ||
        (launch_now && mode == ScreensaverMode::Off) ||
        (launch_now &&
         (current_state_ == UiState::AlarmRinging ||
          current_state_ == UiState::TimerFinished ||
          current_state_ == UiState::Emulator)))
    {
        return false;
    }

    app_settings.screensaver_mode = mode;
    app_settings.screensaver_delay_index =
        delay_index;
    settings_store.saveScreensaverMode(mode);
    settings_store.saveScreensaverDelay(delay_index);
    set_checked_button(
        boot_options_view.screensaver_options,
        static_cast<uint8_t>(mode));
    set_checked_button(
        boot_options_view.screensaver_delay_options,
        delay_index);

    if (launch_now)
    {
        screensaver_launch_pending_ = true;
        if (current_state_ != UiState::Normal)
            requestState(UiState::Normal);
    }
    return true;
}

bool MaclockApp::applyControlAlarm(
    size_t index, const ControlPanelAlarm &alarm)
{
    if (index >= kAlarmCount ||
        alarm.volume >= kAudioVolumeLevelCount)
        return false;

    AlarmSettings settings;
    settings.enabled = alarm.enabled ? 1 : 0;
    settings.hour = alarm.hour;
    settings.minute = alarm.minute;
    settings.weekdays = alarm.weekdays;
    settings.sound = 0;
    settings.volume = alarm.volume;
    settings.one_time = alarm.one_time ? 1 : 0;
    settings.gradual_volume =
        alarm.gradual_volume ? 1 : 0;
    settings.sunrise = alarm.sunrise ? 1 : 0;
    strlcpy(
        settings.label,
        alarm.label,
        sizeof(settings.label));
    const bool configured = alarm_service.configure(
        index, settings, alarm.sound);
    if (configured &&
        current_state_ == UiState::AlarmEditor)
    {
        alarm_view.enter(rtc_now());
        alarm_view.showEditor();
        lv_timer_handler();
    }
    return configured;
}

bool MaclockApp::applyControlTimer(
    const ControlPanelTimer &timer,
    bool start, bool cancel)
{
    if (!timer_service.configure(
            timer.minutes, timer.sound, timer.volume))
    {
        return false;
    }
    if (cancel)
        timer_service.cancel();
    else if (start)
        timer_service.start(timer.minutes);
    if (current_state_ == UiState::TimerEditor)
    {
        timer_view.enter(millis());
        timer_view.show(millis());
        lv_timer_handler();
    }
    return true;
}

bool MaclockApp::applyControlNightMode(
    const NightModeSettings &night_mode)
{
    if (night_mode.start_hour >= 24 ||
        night_mode.end_hour >= 24 ||
        night_mode.screen_off_hour >= 24)
    {
        return false;
    }
    app_settings.night_mode = night_mode;
    settings_store.saveNightMode(night_mode);
    update_night_options_ui();
    last_night_check_ms_ = 0;
    return true;
}

bool MaclockApp::applyControlChime(
    const ChimeSettings &chime,
    const char *sound_path)
{
    if (static_cast<uint8_t>(chime.mode) >=
            static_cast<uint8_t>(ChimeMode::Count) ||
        chime.volume >= kAudioVolumeLevelCount ||
        chime.quiet_start_hour >= 24 ||
        chime.quiet_end_hour >= 24 ||
        !sound_path || !sound_path[0])
    {
        return false;
    }

    app_settings.chime = chime;
    strlcpy(
        g_chime_sound_path,
        SoundSelector::resolvePath(
            sound_path, "/quack.mp3"),
        sizeof(g_chime_sound_path));
    settings_store.saveChime(
        app_settings.chime, g_chime_sound_path);
    update_chime_options_ui();
    return true;
}

bool MaclockApp::applyControlSystemSounds(
    const char *startup_path, uint8_t startup_volume,
    const char *floppy_path, uint8_t floppy_volume)
{
    if (!startup_path || !startup_path[0] ||
        !floppy_path || !floppy_path[0] ||
        !audio_volume_is_level(startup_volume) ||
        !audio_volume_is_level(floppy_volume))
    {
        return false;
    }
    strlcpy(
        g_startup_sound_path,
        SoundSelector::resolvePath(
            startup_path, "/startup.mp3"),
        sizeof(g_startup_sound_path));
    strlcpy(
        g_floppy_sound_path,
        SoundSelector::resolvePath(
            floppy_path, "/floppy.mp3"),
        sizeof(g_floppy_sound_path));
    g_startup_sound_volume = startup_volume;
    g_floppy_sound_volume = floppy_volume;
    settings_store.saveSystemSounds(
        g_startup_sound_path, g_startup_sound_volume,
        g_floppy_sound_path, g_floppy_sound_volume);
    return true;
}

bool MaclockApp::applyControlLocation(
    const char *city, const char *country)
{
    return wifi_service.setLocation(city, country);
}

bool MaclockApp::previewControlSound(
    const char *sound_path, uint8_t volume)
{
    if (!sound_path || !sound_path[0] ||
        !audio_volume_is_level(volume))
        return false;

    strlcpy(
        control_preview_sound_,
        sound_path,
        sizeof(control_preview_sound_));
    control_preview_volume_ = volume;
    control_preview_due_ms_ = millis() + 200;
    control_preview_pending_ = true;
    return true;
}

void MaclockApp::beginControlPanelNetworkTransfer()
{
    if (hal_.isLocal())
        return;
    audio_service.stop();
    audio_service.suspendTask();
    display_service.stopAudioOutput();
}

void MaclockApp::endControlPanelNetworkTransfer()
{
    if (hal_.isLocal())
        return;
    display_service.startAudioOutput();
    audio_service.resumeTask();
}

void MaclockApp::showControlPanelDownload(
    const char *message, uint8_t progress)
{
    if (!ui_shell.boot || !ui_shell.boot_message ||
        !ui_shell.boot_progress)
        return;

    ui_shell.hideAll();
    lv_obj_clear_flag(
        ui_shell.background, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_shell.boot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(
        ui_shell.boot_progress, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(
        ui_shell.corners, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(ui_shell.boot_message, 188, 30);
    lv_obj_set_pos(ui_shell.boot_message, 68, 20);
    lv_label_set_long_mode(
        ui_shell.boot_message, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(
        ui_shell.boot_message, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(
        ui_shell.boot_message,
        message ? tr(message) : "");
    lv_bar_set_value(
        ui_shell.boot_progress,
        progress > 100 ? 100 : progress,
        LV_ANIM_OFF);
    lv_timer_handler();
}

void MaclockApp::hideControlPanelDownload()
{
    if (ui_shell.boot_progress)
        lv_obj_add_flag(
            ui_shell.boot_progress, LV_OBJ_FLAG_HIDDEN);
    if (ui_shell.boot_message)
    {
        lv_obj_set_size(ui_shell.boot_message, 188, 24);
        lv_obj_set_pos(ui_shell.boot_message, 68, 25);
        lv_label_set_long_mode(
            ui_shell.boot_message, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(
            ui_shell.boot_message, LV_TEXT_ALIGN_LEFT, 0);
    }
    ui_shell.updateBootMessage();
    lv_timer_handler();
}

bool MaclockApp::requestControlUpdateCheck()
{
    return update_service.requestCheck();
}

bool MaclockApp::requestControlUpdateInstall()
{
    audio_service.stop();
    control_preview_pending_ = false;
    const bool started_guard =
        !update_network_guard_active_;
    if (started_guard)
    {
        beginControlPanelNetworkTransfer();
        update_network_guard_active_ = true;
    }
    const bool requested =
        update_service.requestInstall();
    if (!requested && started_guard)
    {
        endControlPanelNetworkTransfer();
        update_network_guard_active_ = false;
    }
    return requested;
}

void MaclockApp::dismissControlUpdate(bool ignore_version)
{
    update_service.dismiss(ignore_version);
}

bool MaclockApp::beginControlFirmwareUpload(
    const char *filename)
{
    audio_service.stop();
    control_preview_pending_ = false;
    return update_service.beginManualFirmware(filename);
}

bool MaclockApp::writeControlFirmwareUpload(
    const uint8_t *data, size_t length)
{
    return update_service.writeManualFirmware(data, length);
}

bool MaclockApp::finishControlFirmwareUpload()
{
    return update_service.finishManualFirmware();
}

void MaclockApp::abortControlFirmwareUpload()
{
    update_service.abortManualFirmware();
}

bool MaclockApp::rebootAfterControlUpdate()
{
    return update_service.reboot();
}

bool MaclockApp::applyControlMqtt(
    const MqttSettings &settings,
    const char *new_password,
    bool clear_password)
{
    return mqtt_service.configure(
        settings, new_password, clear_password);
}

bool MaclockApp::playMqttSound(
    const char *path, uint8_t volume)
{
    audio_service.stop();
    return audio_service.play(path, volume);
}

void MaclockApp::setMqttBacklight(uint8_t level)
{
    if (level > kBrightnessMax)
        return;
    settings_store.saveBrightness(level);
    input_service.setEncoderPosition(level);
    g_last_saved_encoder = level;
    last_encoder_save_ms_ = millis();
    analogWrite(TFT_BL_VAR, brightness_to_pwm(level));
}

void MaclockApp::stopMqttSound()
{
    audio_service.stop();
}

bool MaclockApp::controlMqttTimer(bool start)
{
    if (start)
        timer_service.start(timer_service.selectedMinutes());
    else
        timer_service.cancel();
    return true;
}

bool MaclockApp::setMqttScreensaver(
    uint8_t mode, bool launch)
{
    if (mode >= static_cast<uint8_t>(ScreensaverMode::Count))
        return false;
    return applyControlScreensaver(
        static_cast<ScreensaverMode>(mode),
        app_settings.screensaver_delay_index,
        launch);
}

bool MaclockApp::setMqttClockFace(uint8_t face)
{
    if (face >= static_cast<uint8_t>(ClockFace::Count))
        return false;
    return applyControlAppearance(
        app_settings.language,
        static_cast<ClockFace>(face),
        app_settings.clock_theme,
        static_cast<uint8_t>(
            constrain(
                input_service.encoderPosition(),
                0, kBrightnessMax)),
        app_settings.face_customization,
        app_settings.time_format);
}

void MaclockApp::rebootMqttDevice()
{
#ifndef MACLOCK_LOCAL
    delay(100);
    ESP.restart();
#endif
}

bool MaclockApp::mqttTimerActive() const
{
    return timer_service.active();
}

void MaclockApp::showMqttMessage(const MqttMessage &message)
{
    if (current_state_ == UI_STATE_NORMAL &&
        clock_view.screensaver_active)
    {
        clock_view.screensaver_active = false;
        const ClockRenderSnapshot snapshot =
            make_clock_snapshot(millis());
        clock_view.show(snapshot);
        clock_view.update(snapshot);
    }
    full_brightness_until_ms_ = millis() + 2000;
    mqtt_notification_view.show(message);
}

void MaclockApp::hideMqttMessage()
{
    mqtt_notification_view.hide();
}

#endif
