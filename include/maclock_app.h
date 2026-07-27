#pragma once

#include <limits.h>
#include <stdint.h>

#include <lvgl.h>

#include "alarm_ui.h"
#include "app_event_sink.h"
#include "app_types.h"
#include "audio_service.h"
#include "datetime_ui.h"
#include "display_service.h"
#include "i2c_bus.h"
#include "input_service.h"
#include "rtc_service.h"
#include "settings_store.h"
#include "timer_ui.h"
#include "weather_service.h"
#include "wifi_mode.h"

class MaclockApp final : public AppEventSink
{
public:
    MaclockApp();

    void begin();
    void tick();

    void requestState(UiState state) override;
    void adjustRtc(const DateTime &date_time) override;
    void snoozeActiveAlarm() override;
    void dismissActiveAlarm() override;
    void dismissTimer() override;

    // Internal accessors used only by LVGL and emulator compatibility thunks.
    SettingsStore &settingsStore() { return settings_store_; }
    AppSettings &settings() { return settings_; }
    I2cBus &i2cBus() { return i2c_bus_; }
    RtcService &rtc() { return rtc_service_; }
    WeatherService &weather() { return weather_service_; }
    InputService &input() { return input_service_; }
    AudioService &audio() { return audio_service_; }
    WifiService &wifi() { return wifi_service_; }
    AlarmService &alarms() { return alarm_service_; }
    AlarmView &alarmView() { return alarm_view_; }
    TimerService &timer() { return timer_service_; }
    TimerView &timerView() { return timer_view_; }
    DateTimeEditor &dateTimeEditor() { return datetime_editor_; }
    DisplayService &display() { return display_service_; }

private:
    SettingsStore settings_store_;
    AppSettings settings_;
    I2cBus i2c_bus_;
    RtcService rtc_service_;
    WeatherService weather_service_;
    InputService input_service_;
    DisplayService display_service_;
    AudioService audio_service_;
    WifiService wifi_service_;
    AlarmService alarm_service_;
    AlarmView alarm_view_;
    TimerService timer_service_;
    TimerView timer_view_;
    DateTimeEditor datetime_editor_;

    UiState requested_state_ = UiState::None;
    UiState current_state_ = UiState::EmptyScreen;
    UiState last_state_ = UiState::None;
    unsigned long state_start_ms_ = 0;
    int active_alarm_index_ = -1;

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
