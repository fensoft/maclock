#pragma once

#include <stdint.h>

#include "audio_volume.h"
#include "localization.h"
#include "regional_settings.h"

enum class UiState : uint8_t
{
    None = 0,
    EmptyScreen = 1,
    WaitStartupSound = 2,
    WaitFloppy1 = 3,
    WaitFloppy2 = 4,
    FloppyInserted = 5,
    BootPlugins = 6,
    WaitFloppySound = 7,
    Normal = 8,
    SetDateTime = 9,
    Calibration = 10,
    BootOptions = 11,
    Emulator = 12,
    Diagnostics = 13,
    AlarmEditor = 14,
    AlarmRinging = 15,
    TimerEditor = 16,
    TimerFinished = 17,
    WifiSetup = 18
};

enum class BootBrightness : uint8_t
{
    Latest,
    Lowest,
    Highest
};

enum class ClockFace : uint8_t
{
    Macintosh,
    Compact,
    Analog,
    Flip,
    Odometer,
    MacOS8,
    Count
};

enum class ClockTheme : uint8_t
{
    Light,
    Dark,
    Count
};

enum class FaceAccent : uint8_t
{
    Default,
    Red,
    Orange,
    Green,
    Blue,
    Purple,
    Count
};

enum class FaceNumeralSize : uint8_t
{
    Small,
    Default,
    Large,
    Count
};

enum class FlipAnimationSpeed : uint8_t
{
    Slow,
    Normal,
    Fast,
    Count
};

struct FaceCustomizationSettings
{
    FaceAccent accent = FaceAccent::Default;
    FaceNumeralSize numeral_size = FaceNumeralSize::Default;
    bool show_weather = true;
    FlipAnimationSpeed flip_speed = FlipAnimationSpeed::Normal;
};

enum class HourFormat : uint8_t
{
    Hour24,
    Hour12,
    Count
};

struct TimeFormatSettings
{
    HourFormat hour_format = HourFormat::Hour24;
    bool leading_zero = true;
    bool show_seconds = true;
    bool show_weekday = false;
};

enum class ScreensaverMode : uint8_t
{
    Off,
    AfterDark,
    Starfield,
    BouncingMac,
    MatrixRain,
    Pipes,
    FlyingClocks,
    Random,
    FlyingToasters,
    Marquee,
    DigitalRainClock,
    Mystify,
    Aquarium,
    Life,
    Maze,
    ErrorParade,
    RainyWindow,
    Fireworks,
    PhotoSlideshow,
    Count
};

static constexpr uint8_t kScreensaverDelayCount = 4;

enum class ChimeMode : uint8_t
{
    Off,
    Hourly,
    QuarterHour,
    Count
};

enum class NightDisplayState : uint8_t
{
    Normal,
    Dimmed,
    Off
};

struct NightModeSettings
{
    bool enabled = false;
    uint8_t start_hour = 22;
    uint8_t end_hour = 7;
    bool screen_off_enabled = false;
    uint8_t screen_off_hour = 23;
};

struct ChimeSettings
{
    ChimeMode mode = ChimeMode::Off;
    uint8_t sound = 0;
    uint8_t volume = 2;
    bool quiet_enabled = true;
    uint8_t quiet_start_hour = 22;
    uint8_t quiet_end_hour = 7;
};

struct AppSettings
{
    UiLanguage language = UI_LANGUAGE_ENGLISH;
    UiDateFormat date_format = UI_DATE_FORMAT_DMY;
    UiTemperatureUnit temperature_unit = UI_TEMPERATURE_CELSIUS;
    ClockFace clock_face = ClockFace::Macintosh;
    ClockTheme clock_theme = ClockTheme::Light;
    FaceCustomizationSettings face_customization;
    TimeFormatSettings time_format;
    ScreensaverMode screensaver_mode = ScreensaverMode::Off;
    uint8_t screensaver_delay_index = 1;
    BootBrightness boot_brightness = BootBrightness::Latest;
    bool boot_floppy_emulator = false;
    NightModeSettings night_mode;
    ChimeSettings chime;
};
