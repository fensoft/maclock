#ifdef MACLOCK_MQTT_COMBINED_SOURCE

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

#endif
