#ifdef MACLOCK_CONFIGURATION_ARCHIVE_COMBINED_SOURCE
static void append_sound_warning(
    char *path, size_t path_size,
    const char *fallback, const char *field,
    std::vector<String> &warnings)
{
    const char *resolved =
        SoundSelector::resolvePath(path, fallback);
    if (strcasecmp(resolved, path) == 0)
        return;
    warnings.push_back(
        String(field) + " used a missing sound; " +
        fallback + " was selected");
    strlcpy(path, fallback, path_size);
}

static void resolve_restored_sounds(
    ControlPanelConfiguration &configuration,
    std::vector<String> &warnings)
{
    SoundSelector::scan();
    append_sound_warning(
        configuration.startup_sound,
        sizeof(configuration.startup_sound),
        "/startup.mp3", "Startup sound", warnings);
    append_sound_warning(
        configuration.floppy_sound,
        sizeof(configuration.floppy_sound),
        "/floppy.mp3", "Floppy sound", warnings);
    append_sound_warning(
        configuration.chime_sound,
        sizeof(configuration.chime_sound),
        "/quack.mp3", "Chime sound", warnings);
    append_sound_warning(
        configuration.timer.sound,
        sizeof(configuration.timer.sound),
        "/quack.mp3", "Timer sound", warnings);
    for (size_t index = 0;
         index < kControlPanelAlarmCount; ++index)
    {
        char field[24];
        snprintf(
            field, sizeof(field), "Alarm %u sound",
            static_cast<unsigned>(index + 1));
        append_sound_warning(
            configuration.alarms[index].sound,
            sizeof(configuration.alarms[index].sound),
            "/quack.mp3", field, warnings);
    }
}

static void send_result(
    WebServer &server, bool ok, const char *message,
    const std::vector<String> &warnings,
    bool network_changed, int status)
{
    JsonDocument document;
    document["ok"] = ok;
    document["message"] = message;
    document["networkChanged"] = network_changed;
    JsonArray warning_array =
        document["warnings"].to<JsonArray>();
    for (const String &warning : warnings)
        warning_array.add(warning);
    String response;
    serializeJson(document, response);
    server.sendHeader("Cache-Control", "no-store");
    server.send(status, "application/json", response);
}
} // namespace
#endif
