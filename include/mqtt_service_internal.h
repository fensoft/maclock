#pragma once

#include "mqtt_service.h"

#ifndef MACLOCK_LOCAL
#include <MQTT.h>
#include <NetworkClient.h>
#else
#include "local_mqtt_client.h"
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
#else
    LocalMqttClient client;
#endif
    bool connected = false;
    bool connecting = false;
    uint32_t next_retry_ms = 0;
    uint32_t retry_delay_ms = 1000;
    uint32_t discovery_due_ms = 0;
    bool inbound_ready = false;
    String inbound_topic;
    String inbound_payload;
};
