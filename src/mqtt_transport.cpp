#ifdef MACLOCK_MQTT_COMBINED_SOURCE

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


void connected_client(MqttService::State &state)
{
    state.connected = true;
    state.connecting = false;
    state.snapshot.connected = true;
    copy_text(state.snapshot.status, "Connected");
    char availability[80];
    char beacon_topic[80];
    char notification_topic[80];
    char sound_topic[80];
    char volume_topic[80];
    char backlight_topic[80];
    char control_topics[8][80];
    build_topic(availability, sizeof(availability), state, "availability");
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
        "screensaver/set", "screensaver/launch", "reboot"};
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
}

bool connect_client(MqttService::State &state, uint32_t now_ms)
{
#ifndef MACLOCK_LOCAL
    state.network.setConnectionTimeout(250);
    state.client.begin(
        state.settings.host,
        static_cast<int>(state.settings.port),
        state.network);
#else
    state.client.begin(state.settings.host, state.settings.port);
#endif
    state.client.setOptions(30, true, 1000);
    char availability[80];
    build_topic(availability, sizeof(availability), state, "availability");
    state.client.setWill(availability, "offline", true, 1);
    const String client_id =
        String("maclock-") + state.snapshot.device_id + "-client";
    if (!state.client.connect(
            client_id.c_str(), state.settings.username, state.password))
    {
        copy_text(state.snapshot.status, "Connection failed");
        state.next_retry_ms = now_ms + state.retry_delay_ms;
        state.retry_delay_ms = min<uint32_t>(
            state.retry_delay_ms * 2, kMqttMaximumRetryMs);
        return false;
    }
#ifdef MACLOCK_LOCAL
    state.connecting = true;
    return true;
#else
    connected_client(state);
    return true;
#endif
}



#endif
