#ifdef MACLOCK_MQTT_COMBINED_SOURCE

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

#endif
