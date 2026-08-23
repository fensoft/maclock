#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "wifi_mode.h"

static constexpr size_t kMqttHostMaxLength = 64;
static constexpr size_t kMqttUsernameMaxLength = 64;
static constexpr size_t kMqttPasswordMaxLength = 64;
static constexpr size_t kMqttMessageIdMaxLength = 64;
static constexpr size_t kMqttMessageTitleMaxLength = 48;
static constexpr size_t kMqttMessageTextMaxLength = 256;

enum class MqttMessageKind : uint8_t
{
    Beacon,
    Notification
};

struct MqttMessage
{
    MqttMessageKind kind = MqttMessageKind::Beacon;
    char id[kMqttMessageIdMaxLength + 1] = "";
    char title[kMqttMessageTitleMaxLength + 1] = "";
    char message[kMqttMessageTextMaxLength + 1] = "";
    uint16_t timeout_seconds = 0;
};

struct MqttSettings
{
    bool enabled = false;
    char host[kMqttHostMaxLength + 1] = "";
    uint16_t port = 1883;
    char username[kMqttUsernameMaxLength + 1] = "";
};

struct MqttSnapshot
{
    MqttSettings settings;
    bool password_set = false;
    bool connected = false;
    char status[65] = "Disabled";
    char device_id[24] = "";
    char topic_base[48] = "";
    char display_state[16] = "idle";
    char current_id[kMqttMessageIdMaxLength + 1] = "";
    char pending_id[kMqttMessageIdMaxLength + 1] = "";
    char last_id[kMqttMessageIdMaxLength + 1] = "";
    char last_result[24] = "";
    char last_error[65] = "";
    char sound[96] = "/quack.mp3";
    uint8_t sound_volume = 80;
    uint8_t backlight = 6;
    bool do_not_disturb = false;
    bool timer_active = false;
    char screensaver[20] = "Off";
    int32_t wifi_rssi = 0;
    char firmware_version[32] = "";
    bool temperature_valid = false;
    float temperature = 0.0f;
};

class MqttEventSink
{
public:
    virtual ~MqttEventSink() = default;
    virtual void showMqttMessage(const MqttMessage &message) = 0;
    virtual void hideMqttMessage() = 0;
    virtual bool playMqttSound(
        const char *path, uint8_t volume) = 0;
    virtual void setMqttBacklight(uint8_t level) = 0;
    virtual void stopMqttSound() = 0;
    virtual bool controlMqttTimer(bool start) = 0;
    virtual bool setMqttScreensaver(
        uint8_t mode, bool launch) = 0;
    virtual void rebootMqttDevice() = 0;
    virtual bool mqttTimerActive() const = 0;
};

class MqttService
{
public:
    struct State;

    void begin(Preferences &preferences, MqttEventSink &events);
    bool configure(
        const MqttSettings &settings,
        const char *new_password,
        bool clear_password);
    void tick(
        const WifiModeSnapshot &wifi,
        bool display_allowed,
        uint32_t now_ms);
    void stop(bool remove_discovery = false);
    void acknowledgeCurrent();
    bool displayActive() const;
    MqttSnapshot snapshot() const;

    State &state();

private:
    State *state_ = nullptr;
};
