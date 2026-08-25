#ifdef MACLOCK_CONFIGURATION_ARCHIVE_COMBINED_SOURCE
template <typename T>
static bool read_json_uint(
    JsonVariantConst value, uint32_t minimum,
    uint32_t maximum, T &destination)
{
    if (!value.is<int32_t>())
        return false;
    const int32_t number = value.as<int32_t>();
    if (number < 0 ||
        static_cast<uint32_t>(number) < minimum ||
        static_cast<uint32_t>(number) > maximum)
    {
        return false;
    }
    destination = static_cast<T>(number);
    return true;
}

static bool read_json_bool(
    JsonVariantConst value, bool &destination)
{
    if (!value.is<bool>())
        return false;
    destination = value.as<bool>();
    return true;
}

static bool read_json_text(
    JsonVariantConst value, char *destination,
    size_t destination_size, bool allow_empty = true)
{
    if (!destination || destination_size == 0 ||
        !value.is<const char *>())
    {
        return false;
    }
    const char *text = value.as<const char *>();
    if (!text || (!allow_empty && !text[0]) ||
        strlen(text) >= destination_size)
    {
        return false;
    }
    strlcpy(destination, text, destination_size);
    return true;
}

static void serialize_configuration(
    const ControlPanelConfiguration &configuration,
    String &json)
{
    JsonDocument document;
    document["format"] = kArchiveFormat;
    document["version"] = kArchiveVersion;
    document["firmwareVersion"] = MACLOCK_VERSION;
    document["board"] = MACLOCK_BOARD_ID;

    JsonObject settings =
        document["settings"].to<JsonObject>();
    settings["language"] = static_cast<uint8_t>(
        configuration.settings.language);
    settings["dateFormat"] = static_cast<uint8_t>(
        configuration.settings.date_format);
    settings["temperatureUnit"] = static_cast<uint8_t>(
        configuration.settings.temperature_unit);
    settings["customClockFace"] =
        configuration.settings.custom_clock_face;
    settings["loadingScreen"] =
        configuration.settings.loading_screen;
    settings["animationSpeed"] = static_cast<uint8_t>(
        configuration.settings.face_customization.flip_speed);
    settings["colonBlink"] = static_cast<uint8_t>(
        configuration.settings.face_customization.colon_blink);
    settings["continuousSeconds"] =
        configuration.settings.face_customization.continuous_seconds;
    settings["hourFormat"] = static_cast<uint8_t>(
        configuration.settings.time_format.hour_format);
    settings["showSeconds"] =
        configuration.settings.time_format.show_seconds;
    settings["screensaverMode"] = static_cast<uint8_t>(
        configuration.settings.screensaver_mode);
    settings["screensaverDelay"] =
        configuration.settings.screensaver_delay_index;
    settings["bootBrightness"] = static_cast<uint8_t>(
        configuration.settings.boot_brightness);
    settings["bootEmulator"] =
        configuration.settings.boot_floppy_emulator;
    settings["brightness"] = configuration.brightness;

    JsonObject night = settings["night"].to<JsonObject>();
    night["enabled"] =
        configuration.settings.night_mode.enabled;
    night["start"] =
        configuration.settings.night_mode.start_hour;
    night["end"] =
        configuration.settings.night_mode.end_hour;
    night["screenOff"] =
        configuration.settings.night_mode.screen_off_enabled;
    night["offHour"] =
        configuration.settings.night_mode.screen_off_hour;

    JsonObject chime =
        settings["chime"].to<JsonObject>();
    chime["mode"] = static_cast<uint8_t>(
        configuration.settings.chime.mode);
    chime["soundIndex"] =
        configuration.settings.chime.sound;
    chime["sound"] = configuration.chime_sound;
    chime["volume"] =
        configuration.settings.chime.volume;
    chime["quiet"] =
        configuration.settings.chime.quiet_enabled;
    chime["quietStart"] =
        configuration.settings.chime.quiet_start_hour;
    chime["quietEnd"] =
        configuration.settings.chime.quiet_end_hour;

    JsonObject system_sounds =
        settings["systemSounds"].to<JsonObject>();
    system_sounds["startup"] =
        configuration.startup_sound;
    system_sounds["startupVolume"] =
        configuration.startup_volume;
    system_sounds["floppy"] =
        configuration.floppy_sound;
    system_sounds["floppyVolume"] =
        configuration.floppy_volume;

    JsonObject timer =
        settings["timer"].to<JsonObject>();
    timer["minutes"] = configuration.timer.minutes;
    timer["sound"] = configuration.timer.sound;
    timer["volume"] = configuration.timer.volume;

    JsonArray alarms = document["alarms"].to<JsonArray>();
    for (const ControlPanelAlarm &alarm :
         configuration.alarms)
    {
        JsonObject item = alarms.add<JsonObject>();
        item["enabled"] = alarm.enabled != 0;
        item["hour"] = alarm.hour;
        item["minute"] = alarm.minute;
        item["weekdays"] = alarm.weekdays;
        item["sound"] = alarm.sound;
        item["volume"] = alarm.volume;
        item["oneTime"] = alarm.one_time;
        item["gradualVolume"] =
            alarm.gradual_volume;
        item["sunrise"] = alarm.sunrise;
        item["label"] = alarm.label;
    }

    JsonObject wifi = document["wifi"].to<JsonObject>();
    wifi["enabled"] = configuration.wifi.enabled;
    wifi["ssid"] = configuration.wifi.ssid;
    wifi["city"] = configuration.wifi.city;
    wifi["country"] = configuration.wifi.country;
    wifi["coordinatesValid"] =
        configuration.wifi.coordinates_valid;
    wifi["latitude"] = configuration.wifi.latitude;
    wifi["longitude"] = configuration.wifi.longitude;
    wifi["location"] = configuration.wifi.location;
    wifi["timezone"] = configuration.wifi.timezone;
    wifi["utcOffsetSeconds"] =
        configuration.wifi.utc_offset_seconds;

    if (configuration.touch.valid)
    {
        JsonObject touch =
            document["touchCalibration"].to<JsonObject>();
        touch["minX"] = configuration.touch.min_x;
        touch["maxX"] = configuration.touch.max_x;
        touch["minY"] = configuration.touch.min_y;
        touch["maxY"] = configuration.touch.max_y;
    }
    else
    {
        document["touchCalibration"] = nullptr;
    }
    serializeJsonPretty(document, json);
}

static bool deserialize_configuration(
    const String &json,
    ControlPanelConfiguration &configuration,
    String &error)
{
    JsonDocument document;
    const DeserializationError json_error =
        deserializeJson(document, json);
    if (json_error)
    {
        error = "configuration.json is not valid JSON";
        return false;
    }
    if (strcmp(
            document["format"] | "",
            kArchiveFormat) != 0 ||
        (document["version"] | 0) != kArchiveVersion)
    {
        error = "Unsupported Maclock backup format";
        return false;
    }

    JsonObjectConst settings = document["settings"];
    JsonObjectConst night = settings["night"];
    JsonObjectConst chime = settings["chime"];
    JsonObjectConst system_sounds =
        settings["systemSounds"];
    JsonObjectConst timer = settings["timer"];
    if (settings.isNull() || night.isNull() ||
        chime.isNull() || system_sounds.isNull() ||
        timer.isNull())
    {
        error = "The backup is missing settings";
        return false;
    }

    uint8_t value = 0;
    bool boolean = false;
    if (!read_json_uint(
            settings["language"], 0,
            UI_LANGUAGE_COUNT - 1, value))
        goto invalid_settings;
    configuration.settings.language =
        static_cast<UiLanguage>(value);
    if (!read_json_uint(
            settings["dateFormat"], 0,
            UI_DATE_FORMAT_COUNT - 1, value))
        goto invalid_settings;
    configuration.settings.date_format =
        static_cast<UiDateFormat>(value);
    if (!read_json_uint(
            settings["temperatureUnit"], 0,
            UI_TEMPERATURE_UNIT_COUNT - 1, value))
        goto invalid_settings;
    configuration.settings.temperature_unit =
        static_cast<UiTemperatureUnit>(value);
    if (settings["customClockFace"].is<const char *>())
    {
        const char *custom_face = settings["customClockFace"];
        if (strlen(custom_face) >=
            sizeof(configuration.settings.custom_clock_face))
            goto invalid_settings;
        strlcpy(
            configuration.settings.custom_clock_face, custom_face,
            sizeof(configuration.settings.custom_clock_face));
    }
    if (!settings["loadingScreen"].isNull())
    {
        if (!settings["loadingScreen"].is<const char *>())
            goto invalid_settings;
        const char *loading_screen = settings["loadingScreen"];
        if (strlen(loading_screen) >=
            sizeof(configuration.settings.loading_screen))
            goto invalid_settings;
        for (const char *cursor = loading_screen; *cursor; ++cursor)
            if (!((*cursor >= 'a' && *cursor <= 'z') ||
                  (*cursor >= 'A' && *cursor <= 'Z') ||
                  (*cursor >= '0' && *cursor <= '9') ||
                  *cursor == '-' || *cursor == '_'))
                goto invalid_settings;
        strlcpy(
            configuration.settings.loading_screen, loading_screen,
            sizeof(configuration.settings.loading_screen));
    }
    if (!read_json_uint(
            settings["animationSpeed"], 0,
            static_cast<uint8_t>(
                FlipAnimationSpeed::Count) -
                1,
            value))
        goto invalid_settings;
    configuration.settings.face_customization.flip_speed =
        static_cast<FlipAnimationSpeed>(value);
    if (!settings["colonBlink"].isNull())
    {
        if (!read_json_uint(settings["colonBlink"], 0,
                static_cast<uint8_t>(ColonBlinkInterval::Count) - 1, value))
            goto invalid_settings;
        configuration.settings.face_customization.colon_blink =
            static_cast<ColonBlinkInterval>(value);
    }
    if (!settings["continuousSeconds"].isNull())
    {
        if (!read_json_uint(settings["continuousSeconds"], 0, 1, value))
            goto invalid_settings;
        configuration.settings.face_customization.continuous_seconds =
            value != 0;
    }
    if (!read_json_uint(
            settings["hourFormat"], 0,
            static_cast<uint8_t>(HourFormat::Count) - 1,
            value))
        goto invalid_settings;
    configuration.settings.time_format.hour_format =
        static_cast<HourFormat>(value);
    if (!settings["showSeconds"].isNull())
    {
        if (!read_json_uint(settings["showSeconds"], 0, 1, value))
            goto invalid_settings;
        configuration.settings.time_format.show_seconds = value != 0;
    }
    if (!read_json_uint(
            settings["screensaverMode"], 0,
            static_cast<uint8_t>(
                ScreensaverMode::Count) -
                1,
            value))
        goto invalid_settings;
    configuration.settings.screensaver_mode =
        static_cast<ScreensaverMode>(value);
    if (!read_json_uint(
            settings["screensaverDelay"], 0,
            kScreensaverDelayCount - 1, value))
        goto invalid_settings;
    configuration.settings.screensaver_delay_index =
        value;
    if (!read_json_uint(
            settings["bootBrightness"], 0,
            static_cast<uint8_t>(
                BootBrightness::Highest),
            value))
        goto invalid_settings;
    configuration.settings.boot_brightness =
        static_cast<BootBrightness>(value);
    if (!read_json_bool(
            settings["bootEmulator"], boolean))
        goto invalid_settings;
    configuration.settings.boot_floppy_emulator =
        boolean;
    if (!read_json_uint(
            settings["brightness"], 0,
            kBrightnessMax, configuration.brightness))
        goto invalid_settings;

    if (!read_json_bool(night["enabled"], boolean))
        goto invalid_settings;
    configuration.settings.night_mode.enabled = boolean;
    if (!read_json_uint(
            night["start"], 0, 23,
            configuration.settings.night_mode.start_hour) ||
        !read_json_uint(
            night["end"], 0, 23,
            configuration.settings.night_mode.end_hour) ||
        !read_json_bool(
            night["screenOff"], boolean) ||
        !read_json_uint(
            night["offHour"], 0, 23,
            configuration.settings.night_mode
                .screen_off_hour))
        goto invalid_settings;
    configuration.settings.night_mode.screen_off_enabled =
        boolean;

    if (!read_json_uint(
            chime["mode"], 0,
            static_cast<uint8_t>(ChimeMode::Count) - 1,
            value))
        goto invalid_settings;
    configuration.settings.chime.mode =
        static_cast<ChimeMode>(value);
    if (!read_json_uint(
            chime["soundIndex"], 0, 2,
            configuration.settings.chime.sound) ||
        !read_json_text(
            chime["sound"], configuration.chime_sound,
            sizeof(configuration.chime_sound), false) ||
        !read_json_uint(
            chime["volume"], 0,
            kAudioVolumeLevelCount - 1,
            configuration.settings.chime.volume) ||
        !read_json_bool(chime["quiet"], boolean))
        goto invalid_settings;
    configuration.settings.chime.quiet_enabled = boolean;
    if (!read_json_uint(
            chime["quietStart"], 0, 23,
            configuration.settings.chime
                .quiet_start_hour) ||
        !read_json_uint(
            chime["quietEnd"], 0, 23,
            configuration.settings.chime
                .quiet_end_hour))
        goto invalid_settings;

    if (!read_json_text(
            system_sounds["startup"],
            configuration.startup_sound,
            sizeof(configuration.startup_sound), false) ||
        !read_json_uint(
            system_sounds["startupVolume"], 10, 100,
            configuration.startup_volume) ||
        !audio_volume_is_level(
            configuration.startup_volume) ||
        !read_json_text(
            system_sounds["floppy"],
            configuration.floppy_sound,
            sizeof(configuration.floppy_sound), false) ||
        !read_json_uint(
            system_sounds["floppyVolume"], 10, 100,
            configuration.floppy_volume) ||
        !audio_volume_is_level(
            configuration.floppy_volume) ||
        !read_json_uint(
            timer["minutes"], 1, 1440,
            configuration.timer.minutes) ||
        !read_json_text(
            timer["sound"], configuration.timer.sound,
            sizeof(configuration.timer.sound), false) ||
        !read_json_uint(
            timer["volume"], 0,
            kAudioVolumeLevelCount - 1,
            configuration.timer.volume))
        goto invalid_settings;

    {
        JsonArrayConst alarms = document["alarms"];
        if (alarms.isNull() ||
            alarms.size() != kControlPanelAlarmCount)
        {
            error = "The backup must contain three alarms";
            return false;
        }
        size_t index = 0;
        for (JsonObjectConst item : alarms)
        {
            ControlPanelAlarm &alarm =
                configuration.alarms[index++];
            if (!read_json_bool(item["enabled"], boolean))
                goto invalid_alarms;
            alarm.enabled = boolean ? 1 : 0;
            if (!read_json_uint(
                    item["hour"], 0, 23, alarm.hour) ||
                !read_json_uint(
                    item["minute"], 0, 59,
                    alarm.minute) ||
                !read_json_uint(
                    item["weekdays"], 0, 0x7F,
                    alarm.weekdays) ||
                !read_json_text(
                    item["sound"], alarm.sound,
                    sizeof(alarm.sound), false) ||
                !read_json_uint(
                    item["volume"], 0,
                    kAudioVolumeLevelCount - 1,
                    alarm.volume) ||
                !read_json_bool(
                    item["oneTime"], alarm.one_time) ||
                !read_json_bool(
                    item["gradualVolume"],
                    alarm.gradual_volume) ||
                !read_json_bool(
                    item["sunrise"], alarm.sunrise) ||
                !read_json_text(
                    item["label"], alarm.label,
                    sizeof(alarm.label)))
            {
                goto invalid_alarms;
            }
        }
    }

    {
        JsonObjectConst wifi = document["wifi"];
        if (wifi.isNull() ||
            !read_json_bool(
                wifi["enabled"],
                configuration.wifi.enabled) ||
            !read_json_text(
                wifi["ssid"], configuration.wifi.ssid,
                sizeof(configuration.wifi.ssid)) ||
            !read_json_text(
                wifi["city"], configuration.wifi.city,
                sizeof(configuration.wifi.city)) ||
            !read_json_text(
                wifi["country"],
                configuration.wifi.country,
                sizeof(configuration.wifi.country)) ||
            !read_json_bool(
                wifi["coordinatesValid"],
                configuration.wifi.coordinates_valid) ||
            !wifi["latitude"].is<double>() ||
            !wifi["longitude"].is<double>() ||
            !read_json_text(
                wifi["location"],
                configuration.wifi.location,
                sizeof(configuration.wifi.location)) ||
            !read_json_text(
                wifi["timezone"],
                configuration.wifi.timezone,
                sizeof(configuration.wifi.timezone)) ||
            !wifi["utcOffsetSeconds"].is<int32_t>())
        {
            error = "The backup contains invalid Wi-Fi settings";
            return false;
        }
        configuration.wifi.latitude =
            wifi["latitude"].as<double>();
        configuration.wifi.longitude =
            wifi["longitude"].as<double>();
        configuration.wifi.utc_offset_seconds =
            wifi["utcOffsetSeconds"].as<int32_t>();
        String country = configuration.wifi.country;
        country.trim();
        country.toUpperCase();
        if ((country.length() != 0 &&
             country.length() != 2) ||
            configuration.wifi.latitude < -90.0 ||
            configuration.wifi.latitude > 90.0 ||
            configuration.wifi.longitude < -180.0 ||
            configuration.wifi.longitude > 180.0 ||
            !isfinite(configuration.wifi.latitude) ||
            !isfinite(configuration.wifi.longitude))
        {
            error = "The backup contains invalid Wi-Fi settings";
            return false;
        }
        country.toCharArray(
            configuration.wifi.country,
            sizeof(configuration.wifi.country));
    }

    if (document["touchCalibration"].isNull())
    {
        configuration.touch = TouchCalibration();
    }
    else
    {
        JsonObjectConst touch =
            document["touchCalibration"];
        configuration.touch.valid = true;
        if (touch.isNull() ||
            !read_json_uint(
                touch["minX"], 0, UINT16_MAX,
                configuration.touch.min_x) ||
            !read_json_uint(
                touch["maxX"], 0, UINT16_MAX,
                configuration.touch.max_x) ||
            !read_json_uint(
                touch["minY"], 0, UINT16_MAX,
                configuration.touch.min_y) ||
            !read_json_uint(
                touch["maxY"], 0, UINT16_MAX,
                configuration.touch.max_y) ||
            configuration.touch.min_x >=
                configuration.touch.max_x ||
            configuration.touch.min_y >=
                configuration.touch.max_y)
        {
            error = "The backup contains invalid touch calibration";
            return false;
        }
    }
    return true;

invalid_settings:
    error = "The backup contains invalid settings";
    return false;
invalid_alarms:
    error = "The backup contains invalid alarm settings";
    return false;
}
#endif
