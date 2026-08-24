#ifdef MACLOCK_MQTT_COMBINED_SOURCE

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
    if (topic == reboot_topic)
    {
        if (state.events)
            state.events->rebootMqttDevice();
        return;
    }
    if (topic == volume_topic)
    {
        const long requested = strtol(payload.c_str(), nullptr, 10);
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
        const long requested = strtol(payload.c_str(), nullptr, 10);
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



#endif
