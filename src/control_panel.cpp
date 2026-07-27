#include "control_panel.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WebServer.h>

#include "brightness.h"
#include "control_panel_page.h"

struct ControlPanelService::State
{
    ControlPanelEventSink *events = nullptr;
    WebServer server{80};
    bool routes_ready = false;
    bool server_running = false;
    bool mdns_running = false;
};

namespace
{
ControlPanelService *active_control_panel = nullptr;

#define g_events (active_control_panel->state().events)
#define g_server (active_control_panel->state().server)
#define g_routes_ready (active_control_panel->state().routes_ready)
#define g_server_running \
    (active_control_panel->state().server_running)
#define g_mdns_running (active_control_panel->state().mdns_running)

static void send_json(JsonDocument &document, int status = 200)
{
    String response;
    serializeJson(document, response);
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send(status, "application/json", response);
}

static void send_result(
    bool ok, const char *message, int status = 200)
{
    JsonDocument document;
    document["ok"] = ok;
    document["message"] = message;
    send_json(document, status);
}

static bool read_uint(
    const char *name, uint32_t minimum, uint32_t maximum,
    uint32_t &value)
{
    if (!g_server.hasArg(name))
        return false;

    const String text = g_server.arg(name);
    if (!text.length())
        return false;
    for (const char character : text)
    {
        if (character < '0' || character > '9')
            return false;
    }

    const unsigned long parsed = strtoul(text.c_str(), nullptr, 10);
    if (parsed < minimum || parsed > maximum)
        return false;
    value = static_cast<uint32_t>(parsed);
    return true;
}

static bool read_sound_arg(const char *name, String &sound)
{
    if (!g_server.hasArg(name))
        return false;
    sound = g_server.arg(name);
    for (size_t i = 0; i < SoundSelector::count(); ++i)
    {
        const char *path = SoundSelector::pathAt(i);
        if (path && sound.equalsIgnoreCase(path))
        {
            sound = path;
            return true;
        }
    }
    return false;
}

static bool read_sound(String &sound)
{
    return read_sound_arg("sound", sound);
}

static void send_control_page()
{
    g_server.sendHeader(
        "Cache-Control", "no-store, no-cache, must-revalidate");
    g_server.sendHeader("Content-Encoding", "gzip");
    g_server.send_P(
        200, "text/html",
        reinterpret_cast<PGM_P>(kControlPanelPageGzip),
        kControlPanelPageGzipLength);
}

static void send_state()
{
    if (!g_events)
    {
        send_result(false, "Control service is unavailable", 503);
        return;
    }

    const ControlPanelSnapshot snapshot =
        g_events->controlPanelSnapshot();
    JsonDocument document;

    JsonObject appearance =
        document["appearance"].to<JsonObject>();
    appearance["face"] =
        static_cast<uint8_t>(snapshot.settings.clock_face);
    appearance["theme"] =
        static_cast<uint8_t>(snapshot.settings.clock_theme);
    appearance["brightness"] = snapshot.brightness;
    appearance["hourFormat"] =
        static_cast<uint8_t>(
            snapshot.settings.time_format.hour_format);
    appearance["leadingZero"] =
        snapshot.settings.time_format.leading_zero;
    appearance["seconds"] =
        snapshot.settings.time_format.show_seconds;
    appearance["weekday"] =
        snapshot.settings.time_format.show_weekday;

    JsonObject system_sounds =
        document["systemSounds"].to<JsonObject>();
    system_sounds["startup"] = snapshot.startup_sound;
    system_sounds["startupVolume"] =
        snapshot.startup_volume;
    system_sounds["floppy"] = snapshot.floppy_sound;
    system_sounds["floppyVolume"] =
        snapshot.floppy_volume;

    JsonObject night = document["night"].to<JsonObject>();
    night["enabled"] = snapshot.settings.night_mode.enabled;
    night["start"] = snapshot.settings.night_mode.start_hour;
    night["end"] = snapshot.settings.night_mode.end_hour;
    night["screenOff"] =
        snapshot.settings.night_mode.screen_off_enabled;
    night["offHour"] =
        snapshot.settings.night_mode.screen_off_hour;

    JsonObject chime = document["chime"].to<JsonObject>();
    chime["mode"] =
        static_cast<uint8_t>(snapshot.settings.chime.mode);
    chime["sound"] = snapshot.chime_sound;
    chime["volume"] = snapshot.settings.chime.volume;
    chime["quiet"] = snapshot.settings.chime.quiet_enabled;
    chime["quietStart"] =
        snapshot.settings.chime.quiet_start_hour;
    chime["quietEnd"] =
        snapshot.settings.chime.quiet_end_hour;

    JsonArray alarms = document["alarms"].to<JsonArray>();
    for (const ControlPanelAlarm &alarm : snapshot.alarms)
    {
        JsonObject item = alarms.add<JsonObject>();
        item["enabled"] = alarm.enabled != 0;
        item["hour"] = alarm.hour;
        item["minute"] = alarm.minute;
        item["weekdays"] = alarm.weekdays;
        item["sound"] = alarm.sound;
        item["volume"] = alarm.volume;
    }

    JsonObject timer = document["timer"].to<JsonObject>();
    timer["active"] = snapshot.timer.active;
    timer["minutes"] = snapshot.timer.minutes;
    timer["remaining"] = snapshot.timer.remaining_seconds;
    timer["sound"] = snapshot.timer.sound;
    timer["volume"] = snapshot.timer.volume;

    JsonArray sounds = document["sounds"].to<JsonArray>();
    for (size_t i = 0; i < SoundSelector::count(); ++i)
    {
        const char *path = SoundSelector::pathAt(i);
        if (!path)
            continue;
        JsonObject sound = sounds.add<JsonObject>();
        sound["path"] = path;
        sound["name"] =
            String(SoundSelector::displayName(path));
    }

    send_json(document);
}

static void send_status()
{
    if (!g_events)
    {
        send_result(false, "Control service is unavailable", 503);
        return;
    }

    const ControlPanelSnapshot snapshot =
        g_events->controlPanelSnapshot();
    JsonDocument document;
    document["timer"]["active"] = snapshot.timer.active;
    document["timer"]["remaining"] =
        snapshot.timer.remaining_seconds;
    send_json(document);
}

static void apply_appearance()
{
    uint32_t face = 0;
    uint32_t theme = 0;
    uint32_t brightness = 0;
    uint32_t hour_format = 0;
    uint32_t leading_zero = 0;
    uint32_t seconds = 0;
    uint32_t weekday = 0;
    if (!read_uint(
            "face", 0,
            static_cast<uint8_t>(ClockFace::Count) - 1, face) ||
        !read_uint(
            "theme", 0,
            static_cast<uint8_t>(ClockTheme::Count) - 1, theme) ||
        !read_uint("brightness", 0, kBrightnessMax, brightness) ||
        !read_uint(
            "hourFormat", 0,
            static_cast<uint8_t>(HourFormat::Count) - 1,
            hour_format) ||
        !read_uint("leadingZero", 0, 1, leading_zero) ||
        !read_uint("seconds", 0, 1, seconds) ||
        !read_uint("weekday", 0, 1, weekday))
    {
        send_result(false, "Invalid appearance settings", 400);
        return;
    }

    TimeFormatSettings time_format;
    time_format.hour_format =
        static_cast<HourFormat>(hour_format);
    time_format.leading_zero = leading_zero != 0;
    time_format.show_seconds = seconds != 0;
    time_format.show_weekday = weekday != 0;
    const bool applied = g_events &&
        g_events->applyControlAppearance(
            static_cast<ClockFace>(face),
            static_cast<ClockTheme>(theme),
            static_cast<uint8_t>(brightness),
            time_format);
    send_result(
        applied,
        applied ? "Appearance updated" : "Appearance was not updated",
        applied ? 200 : 500);
}

static void apply_alarm()
{
    uint32_t index = 0;
    uint32_t enabled = 0;
    uint32_t hour = 0;
    uint32_t minute = 0;
    uint32_t weekdays = 0;
    uint32_t volume = 0;
    String sound;
    if (!read_uint(
            "index", 0, kControlPanelAlarmCount - 1, index) ||
        !read_uint("enabled", 0, 1, enabled) ||
        !read_uint("hour", 0, 23, hour) ||
        !read_uint("minute", 0, 59, minute) ||
        !read_uint("weekdays", 0, 0x7F, weekdays) ||
        !read_uint("volume", 0, 3, volume) ||
        !read_sound(sound))
    {
        send_result(false, "Invalid alarm settings", 400);
        return;
    }

    ControlPanelAlarm alarm;
    alarm.enabled = static_cast<uint8_t>(enabled);
    alarm.hour = static_cast<uint8_t>(hour);
    alarm.minute = static_cast<uint8_t>(minute);
    alarm.weekdays = static_cast<uint8_t>(weekdays);
    alarm.volume = static_cast<uint8_t>(volume);
    strlcpy(alarm.sound, sound.c_str(), sizeof(alarm.sound));
    const bool applied = g_events &&
        g_events->applyControlAlarm(index, alarm);
    send_result(
        applied,
        applied ? "Alarm saved" : "Alarm was not saved",
        applied ? 200 : 500);
}

static void apply_timer()
{
    uint32_t minutes = 0;
    uint32_t volume = 0;
    String sound;
    const String action = g_server.arg("action");
    const bool start = action == "start";
    const bool cancel = action == "cancel";
    if ((!start && !cancel && action != "save") ||
        !read_uint("minutes", 1, 1440, minutes) ||
        !read_uint("volume", 0, 3, volume) ||
        !read_sound(sound))
    {
        send_result(false, "Invalid timer settings", 400);
        return;
    }

    ControlPanelTimer timer;
    timer.minutes = static_cast<uint16_t>(minutes);
    timer.volume = static_cast<uint8_t>(volume);
    strlcpy(timer.sound, sound.c_str(), sizeof(timer.sound));
    const bool applied = g_events &&
        g_events->applyControlTimer(timer, start, cancel);
    send_result(
        applied,
        cancel ? "Timer cancelled"
               : (start ? "Timer started" : "Timer defaults saved"),
        applied ? 200 : 500);
}

static void apply_night()
{
    uint32_t enabled = 0;
    uint32_t start = 0;
    uint32_t end = 0;
    uint32_t screen_off = 0;
    uint32_t off_hour = 0;
    if (!read_uint("enabled", 0, 1, enabled) ||
        !read_uint("start", 0, 23, start) ||
        !read_uint("end", 0, 23, end) ||
        !read_uint("screenOff", 0, 1, screen_off) ||
        !read_uint("offHour", 0, 23, off_hour))
    {
        send_result(false, "Invalid night-mode settings", 400);
        return;
    }

    NightModeSettings settings;
    settings.enabled = enabled != 0;
    settings.start_hour = static_cast<uint8_t>(start);
    settings.end_hour = static_cast<uint8_t>(end);
    settings.screen_off_enabled = screen_off != 0;
    settings.screen_off_hour = static_cast<uint8_t>(off_hour);
    const bool applied = g_events &&
        g_events->applyControlNightMode(settings);
    send_result(
        applied,
        applied ? "Night mode saved" : "Night mode was not saved",
        applied ? 200 : 500);
}

static void apply_chime()
{
    uint32_t mode = 0;
    uint32_t volume = 0;
    uint32_t quiet = 0;
    uint32_t quiet_start = 0;
    uint32_t quiet_end = 0;
    String sound;
    if (!read_uint(
            "mode", 0,
            static_cast<uint8_t>(ChimeMode::Count) - 1, mode) ||
        !read_uint("volume", 0, 3, volume) ||
        !read_uint("quiet", 0, 1, quiet) ||
        !read_uint("quietStart", 0, 23, quiet_start) ||
        !read_uint("quietEnd", 0, 23, quiet_end) ||
        !read_sound(sound))
    {
        send_result(false, "Invalid chime settings", 400);
        return;
    }

    ChimeSettings settings;
    settings.mode = static_cast<ChimeMode>(mode);
    settings.volume = static_cast<uint8_t>(volume);
    settings.quiet_enabled = quiet != 0;
    settings.quiet_start_hour =
        static_cast<uint8_t>(quiet_start);
    settings.quiet_end_hour =
        static_cast<uint8_t>(quiet_end);
    const bool applied = g_events &&
        g_events->applyControlChime(settings, sound.c_str());
    send_result(
        applied,
        applied ? "Chime settings saved"
                : "Chime settings were not saved",
        applied ? 200 : 500);
}

static void preview_sound()
{
    uint32_t volume = 0;
    String sound;
    if (!read_uint("volume", 1, 100, volume) ||
        !read_sound(sound))
    {
        send_result(false, "Invalid sound preview", 400);
        return;
    }

    const bool started = g_events &&
        g_events->previewControlSound(
            sound.c_str(), static_cast<uint8_t>(volume));
    send_result(
        started,
        started ? "Playing sound" : "Sound could not be played",
        started ? 200 : 500);
}

static void apply_system_sounds()
{
    uint32_t startup_volume = 0;
    uint32_t floppy_volume = 0;
    String startup;
    String floppy;
    if (!read_sound_arg("startup", startup) ||
        !read_uint(
            "startupVolume", 0, 100, startup_volume) ||
        !read_sound_arg("floppy", floppy) ||
        !read_uint(
            "floppyVolume", 0, 100, floppy_volume))
    {
        send_result(false, "Invalid system sound settings", 400);
        return;
    }

    const bool applied = g_events &&
        g_events->applyControlSystemSounds(
            startup.c_str(),
            static_cast<uint8_t>(startup_volume),
            floppy.c_str(),
            static_cast<uint8_t>(floppy_volume));
    send_result(
        applied,
        applied ? "System sounds saved"
                : "System sounds were not saved",
        applied ? 200 : 500);
}

static void configure_routes()
{
    if (g_routes_ready)
        return;
    g_server.on("/", HTTP_GET, send_control_page);
    g_server.on("/api/state", HTTP_GET, send_state);
    g_server.on("/api/status", HTTP_GET, send_status);
    g_server.on("/api/appearance", HTTP_POST, apply_appearance);
    g_server.on("/api/alarm", HTTP_POST, apply_alarm);
    g_server.on("/api/timer", HTTP_POST, apply_timer);
    g_server.on("/api/night", HTTP_POST, apply_night);
    g_server.on("/api/chime", HTTP_POST, apply_chime);
    g_server.on("/api/sounds", HTTP_POST, apply_system_sounds);
    g_server.on("/api/preview", HTTP_POST, preview_sound);
    g_server.onNotFound(
        []()
        {
            send_result(false, "Control-panel route not found", 404);
        });
    g_routes_ready = true;
}
} // namespace

ControlPanelService::State &ControlPanelService::state()
{
    return *state_;
}

void ControlPanelService::begin(ControlPanelEventSink &events)
{
    if (!state_)
        state_ = new State();
    active_control_panel = this;
    g_events = &events;
    configure_routes();
}

void ControlPanelService::tick(const WifiModeSnapshot &wifi)
{
    active_control_panel = this;
    if (!wifi.enabled || !wifi.connected || wifi.portal_active)
    {
        stop();
        return;
    }

    if (!g_server_running)
    {
        g_server.begin();
        g_server_running = true;
        g_mdns_running = MDNS.begin("maclock");
        if (g_mdns_running)
            MDNS.addService("http", "tcp", 80);
        Serial.printf(
            "[Control] http://%s/ or http://maclock.local/\n",
            wifi.ip_address);
    }
    g_server.handleClient();
}

void ControlPanelService::stop()
{
    if (!state_)
        return;
    active_control_panel = this;
    if (g_server_running)
    {
        g_server.stop();
        g_server_running = false;
    }
    if (g_mdns_running)
    {
        MDNS.end();
        g_mdns_running = false;
    }
}

bool ControlPanelService::running() const
{
    return state_ && state_->server_running;
}
