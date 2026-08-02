#pragma once

#include <cstdint>
#include <functional>

struct LocalSimulatorUiModel
{
    void *texture = nullptr;
    uint8_t scale = 1;
    uint8_t backlight_percent = 100;

    uint8_t weather_kind = 0;
    uint8_t weather_address = 0x47;
    bool weather_present = true;
    float temperature = 21.0f;
    float pressure = 1013.0f;
    float humidity = 50.0f;
    std::function<void(
        uint8_t, uint8_t, bool, float, float, float)>
        set_weather;

    bool rtc_ds1307 = false;
    bool rtc_present = true;
    std::function<void(bool, bool)> set_rtc;
    std::function<void()> reset_rtc;

    bool audio_available = false;
    bool muted = true;
    uint32_t audio_rate = 44100;
    uint8_t volume = 0;
    std::function<void(uint8_t)> set_volume;

    uint16_t http_port = 8088;
    bool floppy = false;
    bool emulator_active = false;
    bool touchscreen_present = true;
    std::function<void(bool)> set_touchscreen_present;
    // 0 = settings, 1 = clock, 2 = emulator.
    std::function<void(uint8_t)> reset_maclock;
    std::function<void(bool, float, float)> set_touch;
    std::function<void(int)> encoder_delta;
    std::function<void(bool)> set_floppy;
    std::function<void(bool)> set_alarm;
    std::function<void(bool)> set_clock;
    std::function<void(bool)> set_discrete_touch;
    std::function<int64_t()> encoder_value;
};

void maclock_local_draw_hardware_panel(
    LocalSimulatorUiModel &model);
