#ifdef MACLOCK_COMBINED_SOURCE

ControlPanelSnapshot MaclockApp::controlPanelSnapshot()
{
    ControlPanelSnapshot snapshot;
    snapshot.settings = app_settings;

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
        strlcpy(
            snapshot.alarms[i].sound,
            alarm_service.soundPath(i),
            sizeof(snapshot.alarms[i].sound));
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
    return snapshot;
}

bool MaclockApp::applyControlAppearance(
    ClockFace face, ClockTheme theme, uint8_t brightness,
    const TimeFormatSettings &time_format)
{
    if (static_cast<uint8_t>(face) >=
            static_cast<uint8_t>(ClockFace::Count) ||
        static_cast<uint8_t>(theme) >=
            static_cast<uint8_t>(ClockTheme::Count) ||
        static_cast<uint8_t>(time_format.hour_format) >=
            static_cast<uint8_t>(HourFormat::Count) ||
        brightness > kBrightnessMax)
    {
        return false;
    }

    app_settings.clock_face = face;
    app_settings.clock_theme = theme;
    app_settings.time_format = time_format;
    settings_store.saveClockFace(face);
    settings_store.saveClockTheme(theme);
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

    clock_view.applyTimeFormatLayout();
    clock_view.applyTheme();
    if (current_state_ == UiState::Normal)
    {
        clock_view.last_second = -1;
        const ClockRenderSnapshot snapshot =
            make_clock_snapshot(millis());
        clock_view.show(snapshot);
        clock_view.update(snapshot);
        lv_timer_handler();
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
    const bool configured = alarm_service.configure(
        index, settings, alarm.sound);
    if (configured &&
        current_state_ == UiState::AlarmEditor)
    {
        alarm_view.enter();
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

#endif
