#pragma once

#include <stddef.h>
#include <stdint.h>

#include "alarm_ui.h"
#include "app_types.h"
#include "sound_selector.h"
#include "update_service.h"
#include "wifi_mode.h"

static constexpr size_t kControlPanelAlarmCount = 3;

struct ControlPanelAlarm
{
    uint8_t enabled = 0;
    uint8_t hour = 7;
    uint8_t minute = 0;
    uint8_t weekdays = 0x7F;
    uint8_t volume = kDefaultAudioVolumeIndex;
    bool one_time = false;
    bool gradual_volume = false;
    bool sunrise = false;
    char label[kAlarmLabelMaxLength + 1] = "";
    char sound[SOUND_SELECTOR_PATH_MAX] = "/quack.mp3";
};

struct ControlPanelUpcomingAlarm
{
    bool valid = false;
    bool snoozed = false;
    bool one_time = false;
    uint8_t index = 0;
    uint8_t day_offset = 0;
    uint8_t weekday = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    char label[kAlarmLabelMaxLength + 1] = "";
};

struct ControlPanelTimer
{
    bool active = false;
    uint16_t minutes = 25;
    uint32_t remaining_seconds = 0;
    uint8_t volume = kDefaultAudioVolumeIndex;
    char sound[SOUND_SELECTOR_PATH_MAX] = "/quack.mp3";
};

struct ControlPanelLocation
{
    char city[49] = "";
    char country[3] = "";
    char resolved[49] = "";
    char timezone[41] = "";
};

struct ControlPanelSnapshot
{
    AppSettings settings;
    bool screensaver_active = false;
    uint8_t brightness = 6;
    char startup_sound[SOUND_SELECTOR_PATH_MAX] = "/startup.mp3";
    uint8_t startup_volume = 80;
    char floppy_sound[SOUND_SELECTOR_PATH_MAX] = "/floppy.mp3";
    uint8_t floppy_volume = 60;
    char chime_sound[SOUND_SELECTOR_PATH_MAX] = "/quack.mp3";
    ControlPanelAlarm alarms[kControlPanelAlarmCount];
    ControlPanelUpcomingAlarm upcoming_alarm;
    ControlPanelTimer timer;
    ControlPanelLocation location;
    UpdateSnapshot update;
};

class ControlPanelEventSink
{
public:
    virtual ~ControlPanelEventSink() = default;

    virtual ControlPanelSnapshot controlPanelSnapshot() = 0;
    virtual bool applyControlAppearance(
        UiLanguage language,
        ClockFace face, ClockTheme theme, uint8_t brightness,
        const FaceCustomizationSettings &face_customization,
        const TimeFormatSettings &time_format) = 0;
    virtual bool applyControlScreensaver(
        ScreensaverMode mode, uint8_t delay_index,
        bool launch_now) = 0;
    virtual bool applyControlAlarm(
        size_t index, const ControlPanelAlarm &alarm) = 0;
    virtual bool applyControlTimer(
        const ControlPanelTimer &timer, bool start, bool cancel) = 0;
    virtual bool applyControlNightMode(
        const NightModeSettings &night_mode) = 0;
    virtual bool applyControlChime(
        const ChimeSettings &chime, const char *sound_path) = 0;
    virtual bool applyControlSystemSounds(
        const char *startup_path, uint8_t startup_volume,
        const char *floppy_path, uint8_t floppy_volume) = 0;
    virtual bool applyControlLocation(
        const char *city, const char *country) = 0;
    virtual bool previewControlSound(
        const char *sound_path, uint8_t volume) = 0;
    virtual void beginControlPanelNetworkTransfer() = 0;
    virtual void endControlPanelNetworkTransfer() = 0;
    virtual bool requestControlUpdateCheck() = 0;
    virtual bool requestControlUpdateInstall() = 0;
    virtual void dismissControlUpdate(bool ignore_version) = 0;
    virtual bool beginControlFirmwareUpload(
        const char *filename) = 0;
    virtual bool writeControlFirmwareUpload(
        const uint8_t *data, size_t length) = 0;
    virtual bool finishControlFirmwareUpload() = 0;
    virtual void abortControlFirmwareUpload() = 0;
    virtual bool rebootAfterControlUpdate() = 0;
};

class ControlPanelService
{
public:
    struct State;

    void begin(ControlPanelEventSink &events);
    void tick(const WifiModeSnapshot &wifi);
    void stop();
    bool running() const;

    State &state();

private:
    State *state_ = nullptr;
};
