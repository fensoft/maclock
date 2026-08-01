#include "mqtt_service.h"

#include <ArduinoJson.h>

#include "audio_volume.h"
#include "brightness.h"
#include "maclock_version.h"
#include "sound_selector.h"

#ifndef MACLOCK_LOCAL
#include <Esp.h>
#include <MQTT.h>
#include <NetworkClient.h>
#include <WiFi.h>
#endif

struct MqttService::State
{
    Preferences *preferences = nullptr;
    MqttEventSink *events = nullptr;
    MqttSettings settings;
    MqttSnapshot snapshot;
    char password[kMqttPasswordMaxLength + 1] = "";

    MqttMessage current;
    MqttMessage pending;
    bool has_current = false;
    bool has_pending = false;
    bool current_visible = false;
    uint32_t beacon_due_ms = 0;
    uint32_t beacon_remaining_ms = 0;
    bool status_dirty = true;

#ifndef MACLOCK_LOCAL
    NetworkClient network;
    MQTTClient client{768, 12288};
    bool connected = false;
    uint32_t next_retry_ms = 0;
    uint32_t retry_delay_ms = 1000;
    uint32_t discovery_due_ms = 0;
    bool inbound_ready = false;
    String inbound_topic;
    String inbound_payload;
#endif
};

namespace
{
MqttService *active_mqtt_service = nullptr;
static constexpr uint32_t kMqttInitialRetryMs = 1000;
static constexpr uint32_t kMqttMaximumRetryMs = 60UL * 60UL * 1000UL;
static constexpr const char *kScreensaverNames[] = {
    "Off", "After Dark", "Starfield", "Bouncing Mac",
    "Matrix Rain", "Pipes", "Flying Clocks", "Random",
    "Flying Toasters", "Marquee Message", "Digital Rain Clock",
    "Mystify", "Aquarium", "Game of Life", "Maze", "Error Parade",
    "Rainy Window", "Fireworks", "Photo Slideshow"};
static constexpr const char *kClockFaceNames[] = {
    "Macintosh", "Compact", "Analog", "Flip", "Odometer", "Mac OS 8"};

int option_index(
    const String &value,
    const char *const *options, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (value.equalsIgnoreCase(options[i]))
            return static_cast<int>(i);
    }
    return -1;
}

template <size_t Size>
void copy_text(char (&destination)[Size], const char *source)
{
    strlcpy(destination, source ? source : "", Size);
}

template <size_t Size>
void copy_text(char (&destination)[Size], const String &source)
{
    strlcpy(destination, source.c_str(), Size);
}

bool valid_host(const char *host)
{
    if (!host || !host[0] || strlen(host) > kMqttHostMaxLength)
        return false;
    for (const char *cursor = host; *cursor; ++cursor)
    {
        if (isspace(static_cast<unsigned char>(*cursor)) ||
            *cursor == '/' || *cursor == '\\')
            return false;
    }
    return true;
}

const char *kind_state(MqttMessageKind kind)
{
    return kind == MqttMessageKind::Beacon
               ? "beacon"
               : "notification";
}

void refresh_snapshot(MqttService::State &state)
{
    state.snapshot.settings = state.settings;
    state.snapshot.password_set = state.password[0] != '\0';
#ifndef MACLOCK_LOCAL
    state.snapshot.connected = state.connected;
#else
    state.snapshot.connected = false;
#endif
    copy_text(
        state.snapshot.display_state,
        state.has_current
            ? (state.current_visible
                   ? kind_state(state.current.kind)
                   : "queued")
            : (state.has_pending ? "queued" : "idle"));
    copy_text(
        state.snapshot.current_id,
        state.has_current ? state.current.id : "");
    copy_text(
        state.snapshot.pending_id,
        state.has_pending ? state.pending.id : "");
}

void set_last(
    MqttService::State &state,
    const char *id,
    const char *result,
    const char *error = "")
{
    copy_text(state.snapshot.last_id, id);
    copy_text(state.snapshot.last_result, result);
    copy_text(state.snapshot.last_error, error);
    state.status_dirty = true;
    refresh_snapshot(state);
}

bool duplicate_id(
    const MqttService::State &state,
    const char *id)
{
    return (state.has_current && !strcmp(state.current.id, id)) ||
           (state.has_pending && !strcmp(state.pending.id, id)) ||
           (state.snapshot.last_id[0] &&
            !strcmp(state.snapshot.last_id, id));
}

void queue_message(
    MqttService::State &state,
    const MqttMessage &message)
{
    if (duplicate_id(state, message.id))
    {
        state.status_dirty = true;
        return;
    }
    if (state.has_pending)
    {
        set_last(
            state, state.pending.id, "superseded",
            "Replaced by a newer pending message");
    }
    state.pending = message;
    state.has_pending = true;
    state.status_dirty = true;
    refresh_snapshot(state);
}

bool parse_message(
    MqttService::State &state,
    const String &payload,
    MqttMessageKind kind,
    MqttMessage &message)
{
    JsonDocument document;
    const DeserializationError error =
        deserializeJson(document, payload);
    if (error)
    {
        set_last(state, "", "rejected", "Invalid JSON payload");
        return false;
    }

    JsonVariantConst data = document["data"];
    const char *id = document["id"] | "";
    if (!id[0])
        id = data["id"] | "";
    const char *text = document["message"] | "";
    const char *title = document["title"] | "";
    if (!id[0] || strlen(id) > kMqttMessageIdMaxLength ||
        !text[0] || strlen(text) > kMqttMessageTextMaxLength ||
        strlen(title) > kMqttMessageTitleMaxLength)
    {
        set_last(
            state,
            strlen(id) <= kMqttMessageIdMaxLength ? id : "",
            "rejected", "Invalid id, title, or message");
        return false;
    }

    message.kind = kind;
    copy_text(message.id, id);
    copy_text(
        message.title,
        title[0]
            ? title
            : (kind == MqttMessageKind::Beacon
                   ? "Beacon"
                   : "Notification"));
    copy_text(message.message, text);
    if (kind == MqttMessageKind::Beacon)
    {
        JsonVariantConst timeout_value = document["timeout"];
        if (!timeout_value.is<int>())
            timeout_value = data["timeout"];
        if (!timeout_value.is<int>())
        {
            set_last(state, id, "rejected", "Beacon timeout is required");
            return false;
        }
        const int timeout = timeout_value.as<int>();
        if (timeout < 1 || timeout > 3600)
        {
            set_last(
                state, id, "rejected",
                "Beacon timeout must be between 1 and 3600 seconds");
            return false;
        }
        message.timeout_seconds = static_cast<uint16_t>(timeout);
    }
    return true;
}

#ifndef MACLOCK_LOCAL
void build_topic(
    char *destination, size_t size,
    const MqttService::State &state,
    const char *suffix)
{
    snprintf(
        destination, size, "%s/%s",
        state.snapshot.topic_base, suffix);
}

void mqtt_message_received(String &topic, String &payload)
{
    if (!active_mqtt_service)
        return;
    MqttService::State &state = active_mqtt_service->state();
    state.inbound_topic = topic;
    state.inbound_payload = payload;
    state.inbound_ready = true;
}

void publish_status(MqttService::State &state)
{
    if (!state.connected)
        return;
    refresh_snapshot(state);
    JsonDocument document;
    document["state"] = state.snapshot.display_state;
    document["id"] = state.snapshot.current_id;
    document["pending_id"] = state.snapshot.pending_id;
    if (state.has_current)
    {
        document["title"] = state.current.title;
        document["message"] = state.current.message;
        if (state.current.kind == MqttMessageKind::Beacon)
            document["timeout"] = state.current.timeout_seconds;
    }
    document["last_id"] = state.snapshot.last_id;
    document["last_result"] = state.snapshot.last_result;
    document["last_error"] = state.snapshot.last_error;
    document["sound"] = state.snapshot.sound;
    document["sound_volume"] = state.snapshot.sound_volume;
    document["backlight"] = state.snapshot.backlight;
    document["do_not_disturb"] =
        state.snapshot.do_not_disturb ? "ON" : "OFF";
    document["timer_active"] = state.snapshot.timer_active;
    document["screensaver"] = state.snapshot.screensaver;
    document["clock_face"] = state.snapshot.clock_face;
    document["wifi_rssi"] = state.snapshot.wifi_rssi;
    document["firmware_version"] =
        state.snapshot.firmware_version;
    if (state.snapshot.temperature_valid)
        document["temperature"] = state.snapshot.temperature;
    String payload;
    serializeJson(document, payload);
    char topic[80];
    build_topic(topic, sizeof(topic), state, "status");
    if (state.client.publish(topic, payload, true, 1))
        state.status_dirty = false;
}

void publish_discovery(MqttService::State &state)
{
    if (!state.connected)
        return;
    char availability[80];
    char status[80];
    char beacon[80];
    char notification[80];
    char sound[80];
    char volume[80];
    char backlight[80];
    char stop_sound[80];
    char dismiss[80];
    char dnd[80];
    char timer_start[80];
    char timer_cancel[80];
    char screensaver[80];
    char screensaver_launch[80];
    char clock_face[80];
    char reboot[80];
    build_topic(availability, sizeof(availability), state, "availability");
    build_topic(status, sizeof(status), state, "status");
    build_topic(beacon, sizeof(beacon), state, "beacon/set");
    build_topic(notification, sizeof(notification), state, "notification/set");
    build_topic(sound, sizeof(sound), state, "sound/set");
    build_topic(volume, sizeof(volume), state, "sound/volume/set");
    build_topic(backlight, sizeof(backlight), state, "backlight/set");
    build_topic(stop_sound, sizeof(stop_sound), state, "sound/stop");
    build_topic(dismiss, sizeof(dismiss), state, "notification/dismiss");
    build_topic(dnd, sizeof(dnd), state, "do_not_disturb/set");
    build_topic(timer_start, sizeof(timer_start), state, "timer/start");
    build_topic(timer_cancel, sizeof(timer_cancel), state, "timer/cancel");
    build_topic(screensaver, sizeof(screensaver), state, "screensaver/set");
    build_topic(
        screensaver_launch, sizeof(screensaver_launch),
        state, "screensaver/launch");
    build_topic(clock_face, sizeof(clock_face), state, "clock_face/set");
    build_topic(reboot, sizeof(reboot), state, "reboot");

    JsonDocument document;
    JsonObject device = document["dev"].to<JsonObject>();
    device["ids"] = state.snapshot.device_id;
    device["name"] = "Maclock";
    device["mf"] = "fensoft";
    device["mdl"] = "Maclock";
    device["sn"] = state.snapshot.device_id;
    device["cu"] = String("http://") + WiFi.localIP().toString() + "/";
    JsonObject origin = document["o"].to<JsonObject>();
    origin["name"] = "Maclock";
    origin["url"] = "https://github.com/fensoft/maclock";
    document["avty_t"] = availability;
    document["pl_avail"] = "online";
    document["pl_not_avail"] = "offline";

    JsonObject components = document["cmps"].to<JsonObject>();
    JsonObject status_component =
        components["status"].to<JsonObject>();
    status_component["p"] = "sensor";
    status_component["name"] = "Status";
    status_component["unique_id"] =
        String(state.snapshot.device_id) + "_status";
    status_component["stat_t"] = status;
    status_component["val_tpl"] = "{{ value_json.state }}";
    status_component["json_attr_t"] = status;
    status_component["icon"] = "mdi:message-text-outline";

    JsonObject beacon_component =
        components["beacon"].to<JsonObject>();
    beacon_component["p"] = "notify";
    beacon_component["name"] = "Beacon";
    beacon_component["unique_id"] =
        String(state.snapshot.device_id) + "_beacon";
    beacon_component["cmd_t"] = beacon;
    beacon_component["cmd_tpl"] =
        "{\"id\":\"ha-{{ "
        "now().strftime('%Y%m%d%H%M%S%f') }}\","
        "\"message\":{{ value | to_json }},\"timeout\":15}";
    beacon_component["qos"] = 1;
    beacon_component["retain"] = false;
    beacon_component["icon"] = "mdi:message-badge-outline";

    JsonObject notification_component =
        components["notification"].to<JsonObject>();
    notification_component["p"] = "notify";
    notification_component["name"] = "Notification";
    notification_component["unique_id"] =
        String(state.snapshot.device_id) + "_notification";
    notification_component["cmd_t"] = notification;
    notification_component["cmd_tpl"] =
        "{\"id\":\"ha-{{ "
        "now().strftime('%Y%m%d%H%M%S%f') }}\","
        "\"message\":{{ value | to_json }}}";
    notification_component["qos"] = 1;
    notification_component["retain"] = false;
    notification_component["icon"] = "mdi:message-alert-outline";

    JsonObject sound_component =
        components["sound"].to<JsonObject>();
    sound_component["p"] = "select";
    sound_component["name"] = "Sound";
    sound_component["unique_id"] =
        String(state.snapshot.device_id) + "_sound";
    sound_component["cmd_t"] = sound;
    sound_component["stat_t"] = status;
    sound_component["val_tpl"] = "{{ value_json.sound }}";
    sound_component["icon"] = "mdi:music-note";
    JsonArray sound_options =
        sound_component["options"].to<JsonArray>();
    for (size_t i = 0; i < SoundSelector::count(); ++i)
    {
        const char *path = SoundSelector::pathAt(i);
        if (path)
            sound_options.add(path);
    }

    JsonObject volume_component =
        components["sound_volume"].to<JsonObject>();
    volume_component["p"] = "number";
    volume_component["name"] = "Sound volume";
    volume_component["unique_id"] =
        String(state.snapshot.device_id) + "_sound_volume";
    volume_component["cmd_t"] = volume;
    volume_component["stat_t"] = status;
    volume_component["val_tpl"] =
        "{{ value_json.sound_volume }}";
    volume_component["min"] = 10;
    volume_component["max"] = 100;
    volume_component["step"] = 10;
    volume_component["unit_of_meas"] = "%";
    volume_component["mode"] = "slider";
    volume_component["icon"] = "mdi:volume-high";

    JsonObject backlight_component =
        components["backlight"].to<JsonObject>();
    backlight_component["p"] = "number";
    backlight_component["name"] = "Backlight";
    backlight_component["unique_id"] =
        String(state.snapshot.device_id) + "_backlight";
    backlight_component["cmd_t"] = backlight;
    backlight_component["stat_t"] = status;
    backlight_component["val_tpl"] =
        "{{ value_json.backlight }}";
    backlight_component["min"] = 0;
    backlight_component["max"] = kBrightnessMax;
    backlight_component["step"] = 1;
    backlight_component["mode"] = "slider";
    backlight_component["icon"] = "mdi:brightness-6";

    JsonObject stop_component =
        components["stop_sound"].to<JsonObject>();
    stop_component["p"] = "button";
    stop_component["name"] = "Stop sound";
    stop_component["unique_id"] =
        String(state.snapshot.device_id) + "_stop_sound";
    stop_component["cmd_t"] = stop_sound;
    stop_component["icon"] = "mdi:stop";

    JsonObject dismiss_component =
        components["dismiss_notification"].to<JsonObject>();
    dismiss_component["p"] = "button";
    dismiss_component["name"] = "Dismiss notification";
    dismiss_component["unique_id"] =
        String(state.snapshot.device_id) + "_dismiss_notification";
    dismiss_component["cmd_t"] = dismiss;
    dismiss_component["icon"] = "mdi:notification-clear-all";

    JsonObject dnd_component =
        components["do_not_disturb"].to<JsonObject>();
    dnd_component["p"] = "switch";
    dnd_component["name"] = "Do not disturb";
    dnd_component["unique_id"] =
        String(state.snapshot.device_id) + "_do_not_disturb";
    dnd_component["cmd_t"] = dnd;
    dnd_component["stat_t"] = status;
    dnd_component["val_tpl"] =
        "{{ value_json.do_not_disturb }}";
    dnd_component["icon"] = "mdi:minus-circle";

    JsonObject timer_start_component =
        components["timer_start"].to<JsonObject>();
    timer_start_component["p"] = "button";
    timer_start_component["name"] = "Start timer";
    timer_start_component["unique_id"] =
        String(state.snapshot.device_id) + "_timer_start";
    timer_start_component["cmd_t"] = timer_start;
    timer_start_component["icon"] = "mdi:timer-play";

    JsonObject timer_cancel_component =
        components["timer_cancel"].to<JsonObject>();
    timer_cancel_component["p"] = "button";
    timer_cancel_component["name"] = "Cancel timer";
    timer_cancel_component["unique_id"] =
        String(state.snapshot.device_id) + "_timer_cancel";
    timer_cancel_component["cmd_t"] = timer_cancel;
    timer_cancel_component["icon"] = "mdi:timer-cancel";

    JsonObject screensaver_component =
        components["screensaver"].to<JsonObject>();
    screensaver_component["p"] = "select";
    screensaver_component["name"] = "Screensaver";
    screensaver_component["unique_id"] =
        String(state.snapshot.device_id) + "_screensaver";
    screensaver_component["cmd_t"] = screensaver;
    screensaver_component["stat_t"] = status;
    screensaver_component["val_tpl"] =
        "{{ value_json.screensaver }}";
    screensaver_component["icon"] = "mdi:monitor-shimmer";
    JsonArray screensaver_options =
        screensaver_component["options"].to<JsonArray>();
    for (const char *name : kScreensaverNames)
        screensaver_options.add(name);

    JsonObject launch_component =
        components["screensaver_launch"].to<JsonObject>();
    launch_component["p"] = "button";
    launch_component["name"] = "Launch screensaver";
    launch_component["unique_id"] =
        String(state.snapshot.device_id) + "_screensaver_launch";
    launch_component["cmd_t"] = screensaver_launch;
    launch_component["icon"] = "mdi:monitor-play";

    JsonObject face_component =
        components["clock_face"].to<JsonObject>();
    face_component["p"] = "select";
    face_component["name"] = "Clock face";
    face_component["unique_id"] =
        String(state.snapshot.device_id) + "_clock_face";
    face_component["cmd_t"] = clock_face;
    face_component["stat_t"] = status;
    face_component["val_tpl"] = "{{ value_json.clock_face }}";
    face_component["icon"] = "mdi:clock-digital";
    JsonArray face_options =
        face_component["options"].to<JsonArray>();
    for (const char *name : kClockFaceNames)
        face_options.add(name);

    JsonObject reboot_component =
        components["reboot"].to<JsonObject>();
    reboot_component["p"] = "button";
    reboot_component["name"] = "Reboot";
    reboot_component["unique_id"] =
        String(state.snapshot.device_id) + "_reboot";
    reboot_component["cmd_t"] = reboot;
    reboot_component["ent_cat"] = "config";
    reboot_component["icon"] = "mdi:restart";

    JsonObject rssi_component =
        components["wifi_rssi"].to<JsonObject>();
    rssi_component["p"] = "sensor";
    rssi_component["name"] = "Wi-Fi RSSI";
    rssi_component["unique_id"] =
        String(state.snapshot.device_id) + "_wifi_rssi";
    rssi_component["stat_t"] = status;
    rssi_component["val_tpl"] = "{{ value_json.wifi_rssi }}";
    rssi_component["dev_cla"] = "signal_strength";
    rssi_component["unit_of_meas"] = "dBm";
    rssi_component["ent_cat"] = "diagnostic";

    JsonObject firmware_component =
        components["firmware_version"].to<JsonObject>();
    firmware_component["p"] = "sensor";
    firmware_component["name"] = "Firmware version";
    firmware_component["unique_id"] =
        String(state.snapshot.device_id) + "_firmware_version";
    firmware_component["stat_t"] = status;
    firmware_component["val_tpl"] =
        "{{ value_json.firmware_version }}";
    firmware_component["ent_cat"] = "diagnostic";
    firmware_component["icon"] = "mdi:chip";

    JsonObject temperature_component =
        components["temperature"].to<JsonObject>();
    temperature_component["p"] = "sensor";
    temperature_component["name"] = "Temperature";
    temperature_component["unique_id"] =
        String(state.snapshot.device_id) + "_temperature";
    temperature_component["stat_t"] = status;
    temperature_component["val_tpl"] =
        "{{ value_json.temperature | default(none) }}";
    temperature_component["dev_cla"] = "temperature";
    temperature_component["unit_of_meas"] = "°C";
    temperature_component["stat_cla"] = "measurement";

    JsonObject error_component =
        components["mqtt_error"].to<JsonObject>();
    error_component["p"] = "sensor";
    error_component["name"] = "MQTT error";
    error_component["unique_id"] =
        String(state.snapshot.device_id) + "_mqtt_error";
    error_component["stat_t"] = status;
    error_component["val_tpl"] =
        "{{ value_json.last_error or 'none' }}";
    error_component["ent_cat"] = "diagnostic";
    error_component["icon"] = "mdi:alert-circle-outline";

    String payload;
    serializeJson(document, payload);
    char discovery_topic[96];
    snprintf(
        discovery_topic, sizeof(discovery_topic),
        "homeassistant/device/%s/config",
        state.snapshot.device_id);
    state.client.publish(discovery_topic, payload, true, 1);
}

void finish_current(MqttService::State &state, const char *result);
void promote_pending(MqttService::State &state);

void process_inbound(MqttService::State &state, uint32_t now_ms)
{
    if (!state.inbound_ready)
        return;
    const String topic = state.inbound_topic;
    const String payload = state.inbound_payload;
    state.inbound_ready = false;
    state.inbound_topic.clear();
    state.inbound_payload.clear();

    if (topic == "homeassistant/status")
    {
        if (payload == "online")
            state.discovery_due_ms = now_ms + random(250, 2001);
        return;
    }

    char beacon_topic[80];
    char notification_topic[80];
    char sound_topic[80];
    char volume_topic[80];
    char backlight_topic[80];
    char stop_sound_topic[80];
    char dismiss_topic[80];
    char dnd_topic[80];
    char timer_start_topic[80];
    char timer_cancel_topic[80];
    char screensaver_topic[80];
    char screensaver_launch_topic[80];
    char clock_face_topic[80];
    char reboot_topic[80];
    build_topic(beacon_topic, sizeof(beacon_topic), state, "beacon/set");
    build_topic(
        notification_topic, sizeof(notification_topic),
        state, "notification/set");
    build_topic(sound_topic, sizeof(sound_topic), state, "sound/set");
    build_topic(
        volume_topic, sizeof(volume_topic),
        state, "sound/volume/set");
    build_topic(
        backlight_topic, sizeof(backlight_topic),
        state, "backlight/set");
    build_topic(
        stop_sound_topic, sizeof(stop_sound_topic),
        state, "sound/stop");
    build_topic(
        dismiss_topic, sizeof(dismiss_topic),
        state, "notification/dismiss");
    build_topic(
        dnd_topic, sizeof(dnd_topic),
        state, "do_not_disturb/set");
    build_topic(
        timer_start_topic, sizeof(timer_start_topic),
        state, "timer/start");
    build_topic(
        timer_cancel_topic, sizeof(timer_cancel_topic),
        state, "timer/cancel");
    build_topic(
        screensaver_topic, sizeof(screensaver_topic),
        state, "screensaver/set");
    build_topic(
        screensaver_launch_topic, sizeof(screensaver_launch_topic),
        state, "screensaver/launch");
    build_topic(
        clock_face_topic, sizeof(clock_face_topic),
        state, "clock_face/set");
    build_topic(reboot_topic, sizeof(reboot_topic), state, "reboot");

    if (topic == sound_topic)
    {
        if (state.snapshot.do_not_disturb)
        {
            set_last(
                state, "", "rejected",
                "Sound blocked by do not disturb");
            return;
        }
        for (size_t i = 0; i < SoundSelector::count(); ++i)
        {
            const char *path = SoundSelector::pathAt(i);
            if (path && payload == path)
            {
                copy_text(state.snapshot.sound, path);
                state.preferences->putString("mqtt_sound", path);
                if (state.events)
                    state.events->playMqttSound(
                        path, state.snapshot.sound_volume);
                state.status_dirty = true;
                return;
            }
        }
        set_last(state, "", "rejected", "Unknown sound");
        return;
    }
    if (topic == stop_sound_topic)
    {
        if (state.events)
            state.events->stopMqttSound();
        return;
    }
    if (topic == dismiss_topic)
    {
        finish_current(state, "dismissed");
        promote_pending(state);
        return;
    }
    if (topic == dnd_topic)
    {
        if (payload != "ON" && payload != "OFF")
        {
            set_last(state, "", "rejected", "Invalid do not disturb state");
            return;
        }
        state.snapshot.do_not_disturb = payload == "ON";
        state.preferences->putBool(
            "mqtt_dnd", state.snapshot.do_not_disturb);
        if (state.snapshot.do_not_disturb && state.events)
            state.events->stopMqttSound();
        state.status_dirty = true;
        return;
    }
    if (topic == timer_start_topic || topic == timer_cancel_topic)
    {
        if (state.events &&
            state.events->controlMqttTimer(topic == timer_start_topic))
        {
            state.snapshot.timer_active = topic == timer_start_topic;
            state.status_dirty = true;
        }
        return;
    }
    if (topic == screensaver_topic)
    {
        const int selected = option_index(
            payload, kScreensaverNames,
            sizeof(kScreensaverNames) / sizeof(kScreensaverNames[0]));
        if (selected < 0 || !state.events ||
            !state.events->setMqttScreensaver(
                static_cast<uint8_t>(selected), false))
        {
            set_last(state, "", "rejected", "Invalid screensaver");
            return;
        }
        copy_text(
            state.snapshot.screensaver,
            kScreensaverNames[selected]);
        state.status_dirty = true;
        return;
    }
    if (topic == screensaver_launch_topic)
    {
        const int selected = option_index(
            state.snapshot.screensaver, kScreensaverNames,
            sizeof(kScreensaverNames) / sizeof(kScreensaverNames[0]));
        if (selected <= 0 || !state.events ||
            !state.events->setMqttScreensaver(
                static_cast<uint8_t>(selected), true))
        {
            set_last(
                state, "", "rejected",
                "Select a screensaver before launching");
        }
        return;
    }
    if (topic == clock_face_topic)
    {
        const int selected = option_index(
            payload, kClockFaceNames,
            sizeof(kClockFaceNames) / sizeof(kClockFaceNames[0]));
        if (selected < 0 || !state.events ||
            !state.events->setMqttClockFace(
                static_cast<uint8_t>(selected)))
        {
            set_last(state, "", "rejected", "Invalid clock face");
            return;
        }
        copy_text(state.snapshot.clock_face, kClockFaceNames[selected]);
        state.status_dirty = true;
        return;
    }
    if (topic == reboot_topic)
    {
        if (state.events)
            state.events->rebootMqttDevice();
        return;
    }
    if (topic == volume_topic)
    {
        const long requested = payload.toInt();
        if (requested < 10 || requested > 100)
        {
            set_last(state, "", "rejected", "Invalid sound volume");
            return;
        }
        state.snapshot.sound_volume =
            audio_volume_nearest_level(
                static_cast<uint8_t>(requested));
        state.preferences->putUChar(
            "mqtt_vol", state.snapshot.sound_volume);
        state.status_dirty = true;
        return;
    }
    if (topic == backlight_topic)
    {
        const long requested = payload.toInt();
        if (requested < 0 || requested > kBrightnessMax)
        {
            set_last(state, "", "rejected", "Invalid backlight level");
            return;
        }
        state.snapshot.backlight = static_cast<uint8_t>(requested);
        if (state.events)
            state.events->setMqttBacklight(
                state.snapshot.backlight);
        state.status_dirty = true;
        return;
    }

    MqttMessageKind kind;
    if (topic == beacon_topic)
        kind = MqttMessageKind::Beacon;
    else if (topic == notification_topic)
        kind = MqttMessageKind::Notification;
    else
        return;

    MqttMessage message;
    if (parse_message(state, payload, kind, message))
        queue_message(state, message);
}

bool connect_client(MqttService::State &state, uint32_t now_ms)
{
    state.network.setConnectionTimeout(250);
    state.client.begin(
        state.settings.host,
        static_cast<int>(state.settings.port),
        state.network);
    state.client.setOptions(30, true, 1000);
    char availability[80];
    build_topic(availability, sizeof(availability), state, "availability");
    state.client.setWill(availability, "offline", true, 1);
    const String client_id =
        String("maclock-") + state.snapshot.device_id + "-client";
    const bool connected = state.client.connect(
        client_id.c_str(),
        state.settings.username,
        state.password);
    state.connected = connected;
    state.snapshot.connected = connected;
    if (!connected)
    {
        copy_text(state.snapshot.status, "Connection failed");
        state.next_retry_ms = now_ms + state.retry_delay_ms;
        state.retry_delay_ms = min<uint32_t>(
            state.retry_delay_ms * 2, kMqttMaximumRetryMs);
        return false;
    }

    state.retry_delay_ms = kMqttInitialRetryMs;
    state.next_retry_ms = 0;
    copy_text(state.snapshot.status, "Connected");
    char beacon_topic[80];
    char notification_topic[80];
    char sound_topic[80];
    char volume_topic[80];
    char backlight_topic[80];
    char control_topics[9][80];
    build_topic(beacon_topic, sizeof(beacon_topic), state, "beacon/set");
    build_topic(
        notification_topic, sizeof(notification_topic),
        state, "notification/set");
    build_topic(sound_topic, sizeof(sound_topic), state, "sound/set");
    build_topic(
        volume_topic, sizeof(volume_topic),
        state, "sound/volume/set");
    build_topic(
        backlight_topic, sizeof(backlight_topic),
        state, "backlight/set");
    static constexpr const char *kControlSuffixes[] = {
        "sound/stop", "notification/dismiss",
        "do_not_disturb/set", "timer/start", "timer/cancel",
        "screensaver/set", "screensaver/launch",
        "clock_face/set", "reboot"};
    for (size_t i = 0;
         i < sizeof(kControlSuffixes) / sizeof(kControlSuffixes[0]);
         ++i)
    {
        build_topic(
            control_topics[i], sizeof(control_topics[i]),
            state, kControlSuffixes[i]);
    }
    state.client.subscribe(beacon_topic, 1);
    state.client.subscribe(notification_topic, 1);
    state.client.subscribe(sound_topic, 1);
    state.client.subscribe(volume_topic, 1);
    state.client.subscribe(backlight_topic, 1);
    for (const auto &topic : control_topics)
        state.client.subscribe(topic, 1);
    state.client.subscribe("homeassistant/status", 0);
    state.client.publish(availability, "online", true, 1);
    publish_discovery(state);
    state.status_dirty = true;
    publish_status(state);
    return true;
}
#endif

void promote_pending(MqttService::State &state)
{
    if (state.has_current || !state.has_pending)
        return;
    state.current = state.pending;
    state.has_current = true;
    state.has_pending = false;
    state.current_visible = false;
    state.beacon_remaining_ms =
        state.current.kind == MqttMessageKind::Beacon
            ? static_cast<uint32_t>(state.current.timeout_seconds) * 1000UL
            : 0;
    state.status_dirty = true;
    refresh_snapshot(state);
}

void finish_current(MqttService::State &state, const char *result)
{
    if (!state.has_current)
        return;
    if (state.current_visible && state.events)
        state.events->hideMqttMessage();
    set_last(state, state.current.id, result);
    state.has_current = false;
    state.current_visible = false;
    state.beacon_due_ms = 0;
    state.beacon_remaining_ms = 0;
    refresh_snapshot(state);
}
}

MqttService::State &MqttService::state()
{
    return *state_;
}

void MqttService::begin(
    Preferences &preferences, MqttEventSink &events)
{
    if (!state_)
        state_ = new State();
    active_mqtt_service = this;
    state_->preferences = &preferences;
    state_->events = &events;
    state_->settings.enabled = preferences.getBool("mqtt_on", false);
    copy_text(
        state_->settings.host,
        preferences.getString("mqtt_host", ""));
    state_->settings.port = preferences.getUShort("mqtt_port", 1883);
    if (!state_->settings.port)
        state_->settings.port = 1883;
    copy_text(
        state_->settings.username,
        preferences.getString("mqtt_user", ""));
    copy_text(
        state_->password,
        preferences.getString("mqtt_pass", ""));
    copy_text(
        state_->snapshot.sound,
        preferences.getString("mqtt_sound", "/quack.mp3"));
    state_->snapshot.sound_volume = audio_volume_nearest_level(
        preferences.getUChar("mqtt_vol", 80));
    state_->snapshot.backlight =
        preferences.getUChar("brightness", 6);
    if (state_->snapshot.backlight > kBrightnessMax)
        state_->snapshot.backlight = 6;
    state_->snapshot.do_not_disturb =
        preferences.getBool("mqtt_dnd", false);
    const uint8_t screensaver = preferences.getUChar(
        "screen_mode", 0);
    copy_text(
        state_->snapshot.screensaver,
        screensaver < sizeof(kScreensaverNames) /
                          sizeof(kScreensaverNames[0])
            ? kScreensaverNames[screensaver]
            : kScreensaverNames[0]);
    const uint8_t face = preferences.getUChar("clock_face", 0);
    copy_text(
        state_->snapshot.clock_face,
        face < sizeof(kClockFaceNames) /
                   sizeof(kClockFaceNames[0])
            ? kClockFaceNames[face]
            : kClockFaceNames[0]);
    copy_text(
        state_->snapshot.firmware_version, MACLOCK_VERSION);
#ifndef MACLOCK_LOCAL
    const uint64_t chip_id = ESP.getEfuseMac();
    snprintf(
        state_->snapshot.device_id,
        sizeof(state_->snapshot.device_id),
        "maclock_%012llx",
        static_cast<unsigned long long>(chip_id & 0xFFFFFFFFFFFFULL));
    state_->client.onMessage(mqtt_message_received);
#else
    copy_text(state_->snapshot.device_id, "maclock_simulator");
#endif
    snprintf(
        state_->snapshot.topic_base,
        sizeof(state_->snapshot.topic_base),
        "maclock/%s",
        state_->snapshot.device_id + strlen("maclock_"));
    copy_text(
        state_->snapshot.status,
        state_->settings.enabled ? "Waiting for Wi-Fi" : "Disabled");
    refresh_snapshot(*state_);
}

bool MqttService::configure(
    const MqttSettings &settings,
    const char *new_password,
    bool clear_password)
{
    if (!state_ || !state_->preferences ||
        (settings.enabled && !valid_host(settings.host)) ||
        !settings.port ||
        strlen(settings.username) > kMqttUsernameMaxLength ||
        (new_password && strlen(new_password) > kMqttPasswordMaxLength))
    {
        return false;
    }

    const bool endpoint_changed =
        strcmp(settings.host, state_->settings.host) ||
        settings.port != state_->settings.port;
    if (!settings.enabled || endpoint_changed)
        stop(true);
    else
        stop(false);

    state_->settings = settings;
    state_->preferences->putBool("mqtt_on", settings.enabled);
    state_->preferences->putString("mqtt_host", settings.host);
    state_->preferences->putUShort("mqtt_port", settings.port);
    state_->preferences->putString("mqtt_user", settings.username);
    if (clear_password)
    {
        state_->password[0] = '\0';
        state_->preferences->putString("mqtt_pass", "");
    }
    else if (new_password && new_password[0])
    {
        copy_text(state_->password, new_password);
        state_->preferences->putString("mqtt_pass", new_password);
    }
    copy_text(
        state_->snapshot.status,
        settings.enabled ? "Waiting for Wi-Fi" : "Disabled");
#ifndef MACLOCK_LOCAL
    state_->next_retry_ms = 0;
    state_->retry_delay_ms = kMqttInitialRetryMs;
#endif
    refresh_snapshot(*state_);
    return true;
}

void MqttService::tick(
    const WifiModeSnapshot &wifi,
    bool display_allowed,
    uint32_t now_ms)
{
    if (!state_)
        return;
    active_mqtt_service = this;
    const bool timer_active =
        state_->events && state_->events->mqttTimerActive();
    if (state_->snapshot.wifi_rssi != wifi.rssi ||
        state_->snapshot.temperature_valid != wifi.forecast_valid ||
        (wifi.forecast_valid &&
         state_->snapshot.temperature != wifi.current_temperature) ||
        state_->snapshot.timer_active != timer_active)
    {
        state_->status_dirty = true;
    }
    state_->snapshot.wifi_rssi = wifi.rssi;
    state_->snapshot.temperature_valid = wifi.forecast_valid;
    state_->snapshot.temperature = wifi.current_temperature;
    state_->snapshot.timer_active = timer_active;
    display_allowed =
        display_allowed && !state_->snapshot.do_not_disturb;

#ifndef MACLOCK_LOCAL
    const bool network_ready =
        state_->settings.enabled && valid_host(state_->settings.host) &&
        wifi.enabled && wifi.connected && !wifi.portal_active;
    if (!network_ready)
    {
        if (state_->connected)
            stop(false);
        copy_text(
            state_->snapshot.status,
            !state_->settings.enabled
                ? "Disabled"
                : (!valid_host(state_->settings.host)
                       ? "Broker setup is required"
                       : "Waiting for Wi-Fi"));
    }
    else
    {
        if (!state_->connected &&
            (!state_->next_retry_ms ||
             static_cast<int32_t>(now_ms - state_->next_retry_ms) >= 0))
        {
            copy_text(state_->snapshot.status, "Connecting");
            connect_client(*state_, now_ms);
        }
        if (state_->connected)
        {
            if (!state_->client.loop())
            {
                state_->connected = false;
                state_->snapshot.connected = false;
                copy_text(state_->snapshot.status, "Connection lost");
                state_->next_retry_ms = now_ms + state_->retry_delay_ms;
            }
            else
            {
                process_inbound(*state_, now_ms);
                if (state_->discovery_due_ms &&
                    static_cast<int32_t>(
                        now_ms - state_->discovery_due_ms) >= 0)
                {
                    state_->discovery_due_ms = 0;
                    publish_discovery(*state_);
                    state_->status_dirty = true;
                }
            }
        }
    }
#else
    (void)wifi;
    copy_text(
        state_->snapshot.status,
        state_->settings.enabled
            ? "Saved; MQTT is unavailable in the simulator"
            : "Disabled");
#endif

    promote_pending(*state_);
    if (state_->has_current && state_->current_visible && !display_allowed)
    {
        if (state_->current.kind == MqttMessageKind::Beacon)
        {
            state_->beacon_remaining_ms =
                static_cast<int32_t>(state_->beacon_due_ms - now_ms) > 0
                    ? state_->beacon_due_ms - now_ms
                    : 1;
        }
        if (state_->events)
            state_->events->hideMqttMessage();
        state_->current_visible = false;
        state_->status_dirty = true;
    }
    if (state_->has_current && !state_->current_visible && display_allowed)
    {
        if (state_->events)
            state_->events->showMqttMessage(state_->current);
        state_->current_visible = true;
        if (state_->current.kind == MqttMessageKind::Beacon)
        {
            if (!state_->beacon_remaining_ms)
                state_->beacon_remaining_ms =
                    static_cast<uint32_t>(
                        state_->current.timeout_seconds) * 1000UL;
            state_->beacon_due_ms = now_ms + state_->beacon_remaining_ms;
        }
        state_->status_dirty = true;
    }
    if (state_->has_current && state_->current_visible &&
        state_->current.kind == MqttMessageKind::Beacon &&
        static_cast<int32_t>(now_ms - state_->beacon_due_ms) >= 0)
    {
        finish_current(*state_, "auto_acknowledged");
        promote_pending(*state_);
    }
    refresh_snapshot(*state_);
#ifndef MACLOCK_LOCAL
    if (state_->status_dirty && state_->connected)
        publish_status(*state_);
#endif
}

void MqttService::stop(bool remove_discovery)
{
    if (!state_)
        return;
#ifndef MACLOCK_LOCAL
    if (state_->connected)
    {
        char availability[80];
        build_topic(
            availability, sizeof(availability),
            *state_, "availability");
        if (remove_discovery)
        {
            char discovery_topic[96];
            snprintf(
                discovery_topic, sizeof(discovery_topic),
                "homeassistant/device/%s/config",
                state_->snapshot.device_id);
            state_->client.publish(discovery_topic, "", true, 1);
        }
        state_->client.publish(availability, "offline", true, 1);
        state_->client.disconnect();
    }
    state_->connected = false;
    state_->snapshot.connected = false;
#else
    (void)remove_discovery;
#endif
}

void MqttService::acknowledgeCurrent()
{
    if (!state_ || !state_->has_current ||
        state_->current.kind != MqttMessageKind::Notification)
        return;
    finish_current(*state_, "acknowledged");
    promote_pending(*state_);
#ifndef MACLOCK_LOCAL
    if (state_->connected)
        publish_status(*state_);
#endif
}

bool MqttService::displayActive() const
{
    return state_ && state_->has_current && state_->current_visible;
}

MqttSnapshot MqttService::snapshot() const
{
    if (!state_)
        return MqttSnapshot();
    refresh_snapshot(*state_);
    return state_->snapshot;
}
