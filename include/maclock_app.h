#pragma once

#include <limits.h>
#include <stdint.h>

#include <lvgl.h>

#include "alarm_ui.h"
#include "app_event_sink.h"
#include "app_types.h"
#include "audio_service.h"
#include "control_panel.h"
#include "datetime_ui.h"
#include "display_service.h"
#include "i2c_bus.h"
#include "input_service.h"
#include "maclock_hal.h"
#include "mqtt_service.h"
#include "rotary_navigator.h"
#include "rtc_service.h"
#include "settings_store.h"
#include "timer_ui.h"
#include "update_service.h"
#include "weather_service.h"
#include "wifi_mode.h"

class MaclockApp final
    : public AppEventSink,
      public ControlPanelEventSink,
      public MqttEventSink
{
public:
    explicit MaclockApp(MaclockHal &hal);

#ifdef MACLOCK_LOCAL
    bool showLocalManualPage(uint8_t page) override;
#endif

    void begin();
    void tick();

    void requestState(UiState state) override;
    void adjustRtc(const DateTime &date_time) override;
    void snoozeActiveAlarm() override;
    void dismissActiveAlarm() override;
    void dismissTimer() override;

    ControlPanelSnapshot controlPanelSnapshot() override;
    ControlPanelConfiguration
        controlPanelConfiguration() override;
    bool applyControlConfiguration(
        const ControlPanelConfiguration &configuration,
        bool &network_changed) override;
    bool applyControlAppearance(
        UiLanguage language,
        ClockFace face, uint8_t brightness,
        const FaceCustomizationSettings &face_customization,
        const TimeFormatSettings &time_format,
        const char *custom_clock_face) override;
    bool applyControlScreensaver(
        ScreensaverMode mode, uint8_t delay_index,
        bool launch_now) override;
    bool applyControlAlarm(
        size_t index, const ControlPanelAlarm &alarm) override;
    bool applyControlTimer(
        const ControlPanelTimer &timer,
        bool start, bool cancel) override;
    bool applyControlNightMode(
        const NightModeSettings &night_mode) override;
    bool applyControlChime(
        const ChimeSettings &chime,
        const char *sound_path) override;
    bool applyControlSystemSounds(
        const char *startup_path, uint8_t startup_volume,
        const char *floppy_path, uint8_t floppy_volume) override;
    bool applyControlLocation(
        const char *city, const char *country) override;
    bool previewControlSound(
        const char *sound_path, uint8_t volume) override;
    void beginControlPanelNetworkTransfer() override;
    void endControlPanelNetworkTransfer() override;
    void showControlPanelDownload(
        const char *message, uint8_t progress) override;
    void hideControlPanelDownload() override;
    bool requestControlUpdateCheck() override;
    bool requestControlUpdateInstall() override;
    void dismissControlUpdate(bool ignore_version) override;
    bool beginControlFirmwareUpload(
        const char *filename) override;
    bool writeControlFirmwareUpload(
        const uint8_t *data, size_t length) override;
    bool finishControlFirmwareUpload() override;
    void abortControlFirmwareUpload() override;
    bool rebootAfterControlUpdate() override;
    bool applyControlMqtt(
        const MqttSettings &settings,
        const char *new_password,
        bool clear_password) override;
    bool playMqttSound(
        const char *path, uint8_t volume) override;
    void setMqttBacklight(uint8_t level) override;
    void stopMqttSound() override;
    bool controlMqttTimer(bool start) override;
    bool setMqttScreensaver(
        uint8_t mode, bool launch) override;
    bool setMqttClockFace(uint8_t face) override;
    void rebootMqttDevice() override;
    bool mqttTimerActive() const override;

    void showMqttMessage(const MqttMessage &message) override;
    void hideMqttMessage() override;

    // Internal accessors used only by LVGL and emulator compatibility thunks.
    SettingsStore &settingsStore() { return settings_store_; }
    AppSettings &settings() { return settings_; }
    I2cBus &i2cBus() { return i2c_bus_; }
    RtcService &rtc() { return rtc_service_; }
    WeatherService &weather() { return weather_service_; }
    InputService &input() { return input_service_; }
    AudioService &audio() { return audio_service_; }
    WifiService &wifi() { return wifi_service_; }
    ControlPanelService &controlPanel()
    {
        return control_panel_service_;
    }
    MqttService &mqtt() { return mqtt_service_; }
    UpdateService &updates() { return update_service_; }
    AlarmService &alarms() { return alarm_service_; }
    AlarmView &alarmView() { return alarm_view_; }
    TimerService &timer() { return timer_service_; }
    TimerView &timerView() { return timer_view_; }
    DateTimeEditor &dateTimeEditor() { return datetime_editor_; }
    DisplayService &display() { return display_service_; }
    bool touchscreenAvailable() const
    {
        return touchscreen_available_;
    }

private:
    void syncUpdatePrompt(bool allowed);

    MaclockHal &hal_;
    SettingsStore settings_store_;
    AppSettings settings_;
    I2cBus i2c_bus_;
    RtcService rtc_service_;
    WeatherService weather_service_;
    InputService input_service_;
    DisplayService display_service_;
    AudioService audio_service_;
    WifiService wifi_service_;
    ControlPanelService control_panel_service_;
    MqttService mqtt_service_;
    UpdateService update_service_;
    AlarmService alarm_service_;
    AlarmView alarm_view_;
    TimerService timer_service_;
    TimerView timer_view_;
    DateTimeEditor datetime_editor_;
    RotaryNavigator rotary_navigator_;

    UiState requested_state_ = UiState::None;
    UiState current_state_ = UiState::EmptyScreen;
    UiState last_state_ = UiState::None;
    unsigned long state_start_ms_ = 0;
    bool startup_sound_started_ = false;
    bool floppy_sound_started_ = false;
    int active_alarm_index_ = -1;
    uint8_t active_alarm_volume_ = 0;

    char control_preview_sound_[SOUND_SELECTOR_PATH_MAX] = {};
    uint32_t control_preview_due_ms_ = 0;
    uint8_t control_preview_volume_ = 0;
    bool control_preview_pending_ = false;
    bool screensaver_launch_pending_ = false;
    ScreensaverMode screensaver_preview_mode_ = ScreensaverMode::Off;
    bool update_network_guard_active_ = false;
    bool touchscreen_available_ = true;
    bool rotary_menu_active_ = false;
    int rotary_last_position_ = 0;
    int rotary_brightness_position_ = 0;

    unsigned long last_alarm_check_ms_ = 0;
    unsigned long last_encoder_save_ms_ = 0;
    unsigned long full_brightness_until_ms_ = 0;
    unsigned long last_night_check_ms_ = 0;
    int screensaver_last_encoder_ = INT32_MIN;
    NightDisplayState scheduled_display_state_ =
        NightDisplayState::Normal;

    unsigned long dual_key_hold_start_ms_ = 0;
    bool dual_key_handled_ = false;
    bool clock_press_pending_ = false;
    bool alarm_press_pending_ = false;
};
