#include "control_panel.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include <lvgl.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#if defined(ARDUINO_ARCH_ESP32) && !defined(MACLOCK_LOCAL)
#include <WiFiClient.h>
#include <esp_heap_caps.h>
#include <miniz.h>
#else
#include <zlib.h>
#endif

#include "audio_volume.h"
#include "brightness.h"
#include "configuration_archive.h"
#include "control_panel_page.h"
#include "control_panel_sound_library.h"

LV_FONT_DECLARE(lv_font_chicago_8);
LV_FONT_DECLARE(lv_font_chicago_24);
LV_FONT_DECLARE(lv_font_chicago_32);
LV_FONT_DECLARE(lv_font_chicago_48);
LV_FONT_DECLARE(lv_font_chicago_digits_6);
LV_FONT_DECLARE(lv_font_chicago_digits_10);
LV_FONT_DECLARE(lv_font_chicago_digits_40);
LV_FONT_DECLARE(lv_font_chicago_digits_56);
LV_FONT_DECLARE(lv_font_seven_segment_24);
LV_FONT_DECLARE(lv_font_seven_segment_48);
LV_FONT_DECLARE(lv_font_seven_segment_64);
LV_FONT_DECLARE(lv_font_seven_segment_80);
LV_FONT_DECLARE(lv_font_seven_segment_96);

#include "control_panel_internal.h"

namespace
{

#define g_events (active_control_panel->state().events)
#define g_server (active_control_panel->state().server)
#define g_routes_ready (active_control_panel->state().routes_ready)
#define g_server_running (active_control_panel->state().server_running)
#define g_mdns_running (active_control_panel->state().mdns_running)
#define g_sound_library (active_control_panel->state().sound_library)
#define g_configuration_archive (active_control_panel->state().configuration_archive)
#define g_update_upload_started (active_control_panel->state().update_upload_started)
#define g_update_upload_finished (active_control_panel->state().update_upload_finished)
#define g_update_upload_error (active_control_panel->state().update_upload_error)

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

static bool read_optional_uint(
    const char *name, uint32_t minimum, uint32_t maximum,
    uint32_t &value)
{
    return !g_server.hasArg(name) ||
           read_uint(name, minimum, maximum, value);
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
static void append_mqtt_json(
    JsonObject mqtt, const MqttSnapshot &snapshot)
{
    mqtt["enabled"] = snapshot.settings.enabled;
    mqtt["host"] = snapshot.settings.host;
    mqtt["port"] = snapshot.settings.port;
    mqtt["username"] = snapshot.settings.username;
    mqtt["passwordSet"] = snapshot.password_set;
    mqtt["connected"] = snapshot.connected;
    mqtt["status"] = snapshot.status;
    mqtt["deviceId"] = snapshot.device_id;
    mqtt["topicBase"] = snapshot.topic_base;
    mqtt["displayState"] = snapshot.display_state;
    mqtt["currentId"] = snapshot.current_id;
    mqtt["pendingId"] = snapshot.pending_id;
    mqtt["lastId"] = snapshot.last_id;
    mqtt["lastResult"] = snapshot.last_result;
    mqtt["lastError"] = snapshot.last_error;
    mqtt["sound"] = snapshot.sound;
    mqtt["soundVolume"] = snapshot.sound_volume;
    mqtt["backlight"] = snapshot.backlight;
    mqtt["doNotDisturb"] = snapshot.do_not_disturb;
    mqtt["timerActive"] = snapshot.timer_active;
    mqtt["screensaver"] = snapshot.screensaver;
    mqtt["wifiRssi"] = snapshot.wifi_rssi;
    mqtt["firmwareVersion"] = snapshot.firmware_version;
    mqtt["temperatureValid"] = snapshot.temperature_valid;
    mqtt["temperature"] = snapshot.temperature;
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
    document["touchscreenPresent"] =
        snapshot.touchscreen_present;
    appearance["language"] =
        static_cast<uint8_t>(snapshot.settings.language);
    appearance["customClockFace"] = snapshot.settings.custom_clock_face;
    appearance["animationSpeed"] = static_cast<uint8_t>(
        snapshot.settings.face_customization.flip_speed);
    appearance["colonBlink"] = static_cast<uint8_t>(
        snapshot.settings.face_customization.colon_blink);
    appearance["continuousSeconds"] =
        snapshot.settings.face_customization.continuous_seconds;
    appearance["brightness"] = snapshot.brightness;
    appearance["hourFormat"] =
        static_cast<uint8_t>(
            snapshot.settings.time_format.hour_format);
    appearance["showSeconds"] =
        snapshot.settings.time_format.show_seconds;

    JsonObject screensaver =
        document["screensaver"].to<JsonObject>();
    screensaver["mode"] =
        static_cast<uint8_t>(
            snapshot.settings.screensaver_mode);
    screensaver["delay"] =
        snapshot.settings.screensaver_delay_index;
    screensaver["active"] =
        snapshot.screensaver_active;

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
        item["oneTime"] = alarm.one_time;
        item["gradualVolume"] = alarm.gradual_volume;
        item["sunrise"] = alarm.sunrise;
        item["label"] = alarm.label;
    }
    JsonObject upcoming =
        document["upcomingAlarm"].to<JsonObject>();
    upcoming["valid"] = snapshot.upcoming_alarm.valid;
    upcoming["snoozed"] =
        snapshot.upcoming_alarm.snoozed;
    upcoming["oneTime"] =
        snapshot.upcoming_alarm.one_time;
    upcoming["index"] = snapshot.upcoming_alarm.index;
    upcoming["dayOffset"] =
        snapshot.upcoming_alarm.day_offset;
    upcoming["weekday"] =
        snapshot.upcoming_alarm.weekday;
    upcoming["hour"] = snapshot.upcoming_alarm.hour;
    upcoming["minute"] = snapshot.upcoming_alarm.minute;
    upcoming["label"] = snapshot.upcoming_alarm.label;

    JsonObject timer = document["timer"].to<JsonObject>();
    timer["active"] = snapshot.timer.active;
    timer["minutes"] = snapshot.timer.minutes;
    timer["remaining"] = snapshot.timer.remaining_seconds;
    timer["sound"] = snapshot.timer.sound;
    timer["volume"] = snapshot.timer.volume;

    JsonObject location =
        document["location"].to<JsonObject>();
    location["city"] = snapshot.location.city;
    location["country"] = snapshot.location.country;
    location["resolved"] = snapshot.location.resolved;
    location["timezone"] = snapshot.location.timezone;

    JsonArray sounds = document["sounds"].to<JsonArray>();
    g_sound_library.appendSnapshot(sounds, snapshot);
    const size_t filesystem_total = LittleFS.totalBytes();
    const size_t filesystem_used = LittleFS.usedBytes();
    JsonObject storage =
        document["storage"].to<JsonObject>();
    storage["total"] = filesystem_total;
    storage["used"] = filesystem_used;
    storage["free"] =
        filesystem_total > filesystem_used
            ? filesystem_total - filesystem_used
            : 0;
    append_update_json(
        document["update"].to<JsonObject>(), snapshot.update);
    append_mqtt_json(
        document["mqtt"].to<JsonObject>(), snapshot.mqtt);

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
    document["screensaver"]["active"] =
        snapshot.screensaver_active;
    JsonObject upcoming =
        document["upcomingAlarm"].to<JsonObject>();
    upcoming["valid"] = snapshot.upcoming_alarm.valid;
    upcoming["snoozed"] =
        snapshot.upcoming_alarm.snoozed;
    upcoming["oneTime"] =
        snapshot.upcoming_alarm.one_time;
    upcoming["index"] = snapshot.upcoming_alarm.index;
    upcoming["dayOffset"] =
        snapshot.upcoming_alarm.day_offset;
    upcoming["weekday"] =
        snapshot.upcoming_alarm.weekday;
    upcoming["hour"] = snapshot.upcoming_alarm.hour;
    upcoming["minute"] = snapshot.upcoming_alarm.minute;
    upcoming["label"] = snapshot.upcoming_alarm.label;
    append_update_json(
        document["update"].to<JsonObject>(), snapshot.update);
    append_mqtt_json(
        document["mqtt"].to<JsonObject>(), snapshot.mqtt);
    send_json(document);
}
static void apply_appearance()
{
    uint32_t language = 0;
    uint32_t brightness = 0;
    uint32_t hour_format = 0;
    uint32_t show_seconds = 0;
    FaceCustomizationSettings face_customization;
    String custom_clock_face;
    uint32_t flip_speed =
        static_cast<uint8_t>(face_customization.flip_speed);
    uint32_t colon_blink =
        static_cast<uint8_t>(face_customization.colon_blink);
    uint32_t continuous_seconds =
        face_customization.continuous_seconds ? 1 : 0;
    if (!read_uint(
            "language", 0, UI_LANGUAGE_COUNT - 1, language) ||
        !read_uint("brightness", 0, kBrightnessMax, brightness) ||
         !read_uint(
             "hourFormat", 0,
             static_cast<uint8_t>(HourFormat::Count) - 1,
             hour_format) ||
         !read_uint("showSeconds", 0, 1, show_seconds) ||
        !read_uint(
            "animationSpeed", 0,
             static_cast<uint8_t>(
                 FlipAnimationSpeed::Count) -
                 1,
            flip_speed) ||
         !read_uint("colonBlink", 0,
             static_cast<uint8_t>(ColonBlinkInterval::Count) - 1,
             colon_blink) ||
         !read_uint("continuousSeconds", 0, 1, continuous_seconds))
    {
        send_result(false, "Invalid appearance settings", 400);
        return;
    }
    if (g_server.hasArg("customClockFace"))
        custom_clock_face = safe_clockface_name(g_server.arg("customClockFace"));
    if (g_server.hasArg("customClockFace") &&
        g_server.arg("customClockFace").length() && !custom_clock_face.length())
    {
        send_result(false, "Invalid custom clock face", 400);
        return;
    }

    TimeFormatSettings time_format;
    time_format.hour_format =
        static_cast<HourFormat>(hour_format);
    time_format.show_seconds = show_seconds != 0;
    face_customization.flip_speed =
        static_cast<FlipAnimationSpeed>(flip_speed);
    face_customization.colon_blink =
        static_cast<ColonBlinkInterval>(colon_blink);
    face_customization.continuous_seconds = continuous_seconds != 0;
    const bool applied = g_events &&
        g_events->applyControlAppearance(
            static_cast<UiLanguage>(language),
            static_cast<uint8_t>(brightness),
            face_customization,
            time_format,
            custom_clock_face.c_str());
    send_result(
        applied,
        applied ? "Appearance updated" : "Appearance was not updated",
        applied ? 200 : 500);
}

static void apply_screensaver()
{
    uint32_t mode = 0;
    uint32_t delay = 0;
    const String action = g_server.arg("action");
    const bool launch_now = action == "launch";
    if ((!launch_now && action != "save") ||
        !read_uint(
            "mode", 0,
            static_cast<uint8_t>(
                ScreensaverMode::Count) -
                1,
            mode) ||
        !read_uint(
            "delay", 0,
            kScreensaverDelayCount - 1,
            delay))
    {
        send_result(
            false, "Invalid screensaver settings", 400);
        return;
    }

    const bool applied = g_events &&
        g_events->applyControlScreensaver(
            static_cast<ScreensaverMode>(mode),
            static_cast<uint8_t>(delay),
            launch_now);
    send_result(
        applied,
        applied
            ? (launch_now
                   ? "Screensaver launched"
                   : "Screensaver settings saved")
            : "Screensaver settings were not applied",
        applied ? 200 : 500);
}

#ifdef MACLOCK_LOCAL
static void show_local_manual_page()
{
    uint32_t page = 0;
    if (!read_uint("page", 0, 20, page) || !g_events ||
        !g_events->showLocalManualPage(static_cast<uint8_t>(page)))
    {
        send_result(false, "Invalid manual page", 400);
        return;
    }
    send_result(true, "Manual page displayed", 200);
}
#endif

static void apply_location()
{
    String city = g_server.arg("city");
    String country = g_server.arg("country");
    city.trim();
    country.trim();
    country.toUpperCase();
    const bool country_valid =
        !country.length() ||
        (country.length() == 2 &&
         country[0] >= 'A' && country[0] <= 'Z' &&
         country[1] >= 'A' && country[1] <= 'Z');
    if (!city.length() || city.length() > 48 ||
        !country_valid)
    {
        send_result(false, "Invalid location settings", 400);
        return;
    }

    const bool applied = g_events &&
        g_events->applyControlLocation(
            city.c_str(), country.c_str());
    send_result(
        applied,
        applied ? "Location update started"
                : "Location was not updated",
        applied ? 200 : 500);
}

static void apply_mqtt()
{
    uint32_t enabled = 0;
    uint32_t port = 0;
    uint32_t clear_password = 0;
    String host = g_server.arg("host");
    String username = g_server.arg("username");
    String password = g_server.arg("password");
    host.trim();
    username.trim();

    if (!read_uint("enabled", 0, 1, enabled) ||
        !read_uint("port", 1, 65535, port) ||
        !read_uint("clearPassword", 0, 1, clear_password) ||
        host.length() > kMqttHostMaxLength ||
        username.length() > kMqttUsernameMaxLength ||
        password.length() > kMqttPasswordMaxLength ||
        (enabled != 0 && !host.length()))
    {
        send_result(false, "Invalid MQTT settings", 400);
        return;
    }

    MqttSettings settings;
    settings.enabled = enabled != 0;
    settings.port = static_cast<uint16_t>(port);
    strlcpy(settings.host, host.c_str(), sizeof(settings.host));
    strlcpy(
        settings.username, username.c_str(),
        sizeof(settings.username));
    const char *new_password =
        password.length() ? password.c_str() : nullptr;
    const bool applied = g_events &&
        g_events->applyControlMqtt(
            settings, new_password, clear_password != 0);
    send_result(
        applied,
        applied ? "MQTT settings saved"
                : "MQTT settings were not saved",
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
    uint32_t one_time = 0;
    uint32_t gradual_volume = 0;
    uint32_t sunrise = 0;
    String sound;
    String label = g_server.arg("label");
    label.trim();
    if (!read_uint(
            "index", 0, kControlPanelAlarmCount - 1, index) ||
        !read_uint("enabled", 0, 1, enabled) ||
        !read_uint("hour", 0, 23, hour) ||
        !read_uint("minute", 0, 59, minute) ||
        !read_uint("weekdays", 0, 0x7F, weekdays) ||
        !read_uint(
            "volume", 0, kAudioVolumeLevelCount - 1, volume) ||
        !read_uint("oneTime", 0, 1, one_time) ||
        !read_uint(
            "gradualVolume", 0, 1, gradual_volume) ||
        !read_uint("sunrise", 0, 1, sunrise) ||
        label.length() > kAlarmLabelMaxLength ||
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
    alarm.one_time = one_time != 0;
    alarm.gradual_volume = gradual_volume != 0;
    alarm.sunrise = sunrise != 0;
    strlcpy(
        alarm.label, label.c_str(), sizeof(alarm.label));
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
        !read_uint(
            "volume", 0, kAudioVolumeLevelCount - 1, volume) ||
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
        !read_uint(
            "volume", 0, kAudioVolumeLevelCount - 1, volume) ||
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
    if (!read_uint("volume", 10, 100, volume) ||
        !audio_volume_is_level(static_cast<uint8_t>(volume)) ||
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
            "startupVolume", 10, 100, startup_volume) ||
        !read_sound_arg("floppy", floppy) ||
        !read_uint(
            "floppyVolume", 10, 100, floppy_volume) ||
        !audio_volume_is_level(
            static_cast<uint8_t>(startup_volume)) ||
        !audio_volume_is_level(
            static_cast<uint8_t>(floppy_volume)))
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

void register_control_panel_settings_routes_impl(WebServer &server) {
    server.on("/api/state", HTTP_GET, send_state);
    server.on("/api/status", HTTP_GET, send_status);
    server.on("/api/appearance", HTTP_POST, apply_appearance);
#ifdef MACLOCK_LOCAL
    server.on("/api/manual/page", HTTP_POST, show_local_manual_page);
#endif
    server.on("/api/location", HTTP_POST, apply_location);
    server.on("/api/mqtt", HTTP_POST, apply_mqtt);
    server.on("/api/screensaver", HTTP_POST, apply_screensaver);
    server.on("/api/alarm", HTTP_POST, apply_alarm);
    server.on("/api/timer", HTTP_POST, apply_timer);
    server.on("/api/night", HTTP_POST, apply_night);
    server.on("/api/chime", HTTP_POST, apply_chime);
    server.on("/api/sounds", HTTP_POST, apply_system_sounds);
    server.on("/api/preview", HTTP_POST, preview_sound);
}
} // namespace

void register_control_panel_settings_routes(WebServer &server)
{
    register_control_panel_settings_routes_impl(server);
}
