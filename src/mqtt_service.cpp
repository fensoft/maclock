#include "mqtt_service.h"
#include "mqtt_service_internal.h"

#include <ArduinoJson.h>

#include "audio_volume.h"
#include "brightness.h"
#include "maclock_version.h"
#include "sound_selector.h"
#include <WiFi.h>

#ifndef MACLOCK_LOCAL
#include <Esp.h>
#endif

namespace
{
#define MACLOCK_MQTT_COMBINED_SOURCE
MqttService *active_mqtt_service = nullptr;
static constexpr uint32_t kMqttInitialRetryMs = 1000;
static constexpr uint32_t kMqttMaximumRetryMs = 60UL * 60UL * 1000UL;
static constexpr const char *kScreensaverNames[] = {
    "Off", "After Dark", "Starfield", "Bouncing Mac",
    "Matrix Rain", "Pipes", "Flying Clocks", "Random",
    "Flying Toasters", "Marquee Message", "Digital Rain Clock",
    "Mystify", "Aquarium", "Game of Life", "Maze", "Error Parade",
    "Rainy Window", "Fireworks", "Photo Slideshow"};

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
    state.snapshot.connected = state.connected;
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

void build_topic(
    char *destination, size_t size,
    const MqttService::State &state,
    const char *suffix);
bool parse_message(
    MqttService::State &state,
    const String &payload,
    MqttMessageKind kind,
    MqttMessage &message);
void queue_message(
    MqttService::State &state,
    const MqttMessage &message);
void finish_current(MqttService::State &state, const char *result);
void promote_pending(MqttService::State &state);

#include "mqtt_home_assistant.cpp"
#include "mqtt_commands.cpp"
#include "mqtt_transport.cpp"
#include "mqtt_message_queue.cpp"


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
    state_->client.onMessage(mqtt_message_received);
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
    state_->next_retry_ms = 0;
    state_->retry_delay_ms = kMqttInitialRetryMs;
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
#else
    const bool network_ready =
        state_->settings.enabled && valid_host(state_->settings.host);
#endif
    if (!network_ready)
    {
        if (state_->connected || state_->connecting)
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
        if (!state_->connected && !state_->connecting &&
            (!state_->next_retry_ms ||
             static_cast<int32_t>(now_ms - state_->next_retry_ms) >= 0))
        {
            copy_text(state_->snapshot.status, "Connecting");
            connect_client(*state_, now_ms);
        }
        if (state_->connecting)
        {
            if (!state_->client.loop())
            {
                state_->connecting = false;
                state_->snapshot.connected = false;
                copy_text(state_->snapshot.status, "Connection failed");
                state_->next_retry_ms = now_ms + state_->retry_delay_ms;
                state_->retry_delay_ms = min<uint32_t>(
                    state_->retry_delay_ms * 2, kMqttMaximumRetryMs);
            }
#ifdef MACLOCK_LOCAL
            else if (state_->client.connected())
                connected_client(*state_);
#endif
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
    if (state_->status_dirty && state_->connected)
        publish_status(*state_);
}

void MqttService::stop(bool remove_discovery)
{
    if (!state_)
        return;
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
    }
    if (state_->connected || state_->connecting)
        state_->client.disconnect();
    state_->connected = false;
    state_->connecting = false;
    state_->snapshot.connected = false;
}

void MqttService::acknowledgeCurrent()
{
    if (!state_ || !state_->has_current ||
        state_->current.kind != MqttMessageKind::Notification)
        return;
    finish_current(*state_, "acknowledged");
    promote_pending(*state_);
    if (state_->connected)
        publish_status(*state_);
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
