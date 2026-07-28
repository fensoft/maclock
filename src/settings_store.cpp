#include "settings_store.h"

#include "audio_volume.h"
#include "brightness.h"

namespace
{
SettingsStore *g_settings_store = nullptr;

template <typename Enum>
Enum load_enum(Preferences &preferences,
               const char *key,
               Enum fallback,
               Enum count)
{
    const uint8_t saved =
        preferences.getUChar(key, static_cast<uint8_t>(fallback));
    return saved < static_cast<uint8_t>(count)
               ? static_cast<Enum>(saved)
               : fallback;
}
}

bool SettingsStore::begin()
{
    g_settings_store = this;
    if (!preferences_.begin("maclock", false))
        return false;

    static constexpr uint8_t kCurrentVolumeScale = 2;
    const uint8_t saved_scale =
        preferences_.getUChar("volume_scale", 1);
    if (saved_scale < kCurrentVolumeScale)
    {
        if (preferences_.isKey("chime_volume"))
        {
            preferences_.putUChar(
                "chime_volume",
                audio_volume_legacy_index(
                    preferences_.getUChar("chime_volume", 1)));
        }
        if (preferences_.isKey("timer_volume"))
        {
            preferences_.putUChar(
                "timer_volume",
                audio_volume_legacy_index(
                    preferences_.getUChar("timer_volume", 2)));
        }
        if (preferences_.isKey("startup_volume"))
        {
            preferences_.putUChar(
                "startup_volume",
                audio_volume_nearest_level(
                    preferences_.getUChar("startup_volume", 80)));
        }
        if (preferences_.isKey("floppy_volume"))
        {
            preferences_.putUChar(
                "floppy_volume",
                audio_volume_nearest_level(
                    preferences_.getUChar("floppy_volume", 60)));
        }
        preferences_.putUChar(
            "volume_scale", kCurrentVolumeScale);
    }
    return true;
}

AppSettings SettingsStore::load()
{
    AppSettings settings;
    const uint8_t saved_language = preferences_.getUChar(
        "language", UI_LANGUAGE_ENGLISH);
    settings.language =
        saved_language < UI_LANGUAGE_COUNT
            ? static_cast<UiLanguage>(saved_language)
            : UI_LANGUAGE_ENGLISH;
    const uint8_t saved_date_format = preferences_.getUChar(
        "date_format", UI_DATE_FORMAT_DMY);
    settings.date_format =
        saved_date_format < UI_DATE_FORMAT_COUNT
            ? static_cast<UiDateFormat>(saved_date_format)
            : UI_DATE_FORMAT_DMY;
    const uint8_t saved_temperature_unit = preferences_.getUChar(
        "temp_unit", UI_TEMPERATURE_CELSIUS);
    settings.temperature_unit =
        saved_temperature_unit < UI_TEMPERATURE_UNIT_COUNT
            ? static_cast<UiTemperatureUnit>(saved_temperature_unit)
            : UI_TEMPERATURE_CELSIUS;
    settings.clock_face = load_enum(
        preferences_, "clock_face",
        ClockFace::Macintosh, ClockFace::Count);
    settings.clock_theme = load_enum(
        preferences_, "clock_theme",
        ClockTheme::Light, ClockTheme::Count);
    settings.time_format.hour_format = load_enum(
        preferences_, "hour_format",
        HourFormat::Hour24, HourFormat::Count);
    settings.time_format.leading_zero =
        preferences_.getBool("lead_zero", true);
    settings.time_format.show_seconds =
        preferences_.getBool("show_seconds", true);
    settings.time_format.show_weekday =
        preferences_.getBool("show_weekday", false);
    settings.screensaver_mode = load_enum(
        preferences_, "screen_mode",
        ScreensaverMode::Off, ScreensaverMode::Count);
    settings.screensaver_delay_index =
        preferences_.getUChar("screen_delay", 1);
    if (settings.screensaver_delay_index >=
        kScreensaverDelayCount)
        settings.screensaver_delay_index = 1;
    settings.boot_brightness = load_enum(
        preferences_, "boot_brightness",
        BootBrightness::Latest,
        static_cast<BootBrightness>(3));
    settings.boot_floppy_emulator =
        preferences_.getBool("floppy_emulator", true);

    settings.night_mode.enabled =
        preferences_.getBool("night_enabled", false);
    settings.night_mode.start_hour =
        preferences_.getUChar("night_start", 22);
    settings.night_mode.end_hour =
        preferences_.getUChar("night_end", 7);
    settings.night_mode.screen_off_enabled =
        preferences_.getBool("night_off", false);
    settings.night_mode.screen_off_hour =
        preferences_.getUChar("night_off_at", 23);
    if (settings.night_mode.start_hour >= 24)
        settings.night_mode.start_hour = 22;
    if (settings.night_mode.end_hour >= 24)
        settings.night_mode.end_hour = 7;
    if (settings.night_mode.screen_off_hour >= 24)
        settings.night_mode.screen_off_hour = 23;

    settings.chime.mode = load_enum(
        preferences_, "chime_mode",
        ChimeMode::Off, ChimeMode::Count);
    settings.chime.sound = preferences_.getUChar("chime_sound", 0);
    settings.chime.volume = preferences_.getUChar("chime_volume", 2);
    settings.chime.quiet_enabled =
        preferences_.getBool("chime_quiet", true);
    settings.chime.quiet_start_hour =
        preferences_.getUChar("quiet_start", 22);
    settings.chime.quiet_end_hour =
        preferences_.getUChar("quiet_end", 7);
    if (settings.chime.sound >= 3)
        settings.chime.sound = 0;
    if (settings.chime.volume >= kAudioVolumeLevelCount)
        settings.chime.volume = 2;
    if (settings.chime.quiet_start_hour >= 24)
        settings.chime.quiet_start_hour = 22;
    if (settings.chime.quiet_end_hour >= 24)
        settings.chime.quiet_end_hour = 7;
    return settings;
}

Preferences &SettingsStore::preferences()
{
    return preferences_;
}

void SettingsStore::saveLanguage(UiLanguage value)
{
    preferences_.putUChar("language", static_cast<uint8_t>(value));
}

void SettingsStore::saveDateFormat(UiDateFormat value)
{
    preferences_.putUChar("date_format", static_cast<uint8_t>(value));
}

void SettingsStore::saveTemperatureUnit(UiTemperatureUnit value)
{
    preferences_.putUChar("temp_unit", static_cast<uint8_t>(value));
}

void SettingsStore::saveClockFace(ClockFace value)
{
    preferences_.putUChar("clock_face", static_cast<uint8_t>(value));
}

void SettingsStore::saveClockTheme(ClockTheme value)
{
    preferences_.putUChar("clock_theme", static_cast<uint8_t>(value));
}

void SettingsStore::saveTimeFormat(
    const TimeFormatSettings &value)
{
    preferences_.putUChar(
        "hour_format", static_cast<uint8_t>(value.hour_format));
    preferences_.putBool("lead_zero", value.leading_zero);
    preferences_.putBool("show_seconds", value.show_seconds);
    preferences_.putBool("show_weekday", value.show_weekday);
}

void SettingsStore::saveScreensaverMode(ScreensaverMode value)
{
    preferences_.putUChar("screen_mode", static_cast<uint8_t>(value));
}

void SettingsStore::saveScreensaverDelay(uint8_t value)
{
    preferences_.putUChar("screen_delay", value);
}

void SettingsStore::saveBootBrightness(BootBrightness value)
{
    preferences_.putUChar(
        "boot_brightness", static_cast<uint8_t>(value));
}

void SettingsStore::saveBootMode(bool emulator)
{
    preferences_.putBool("floppy_emulator", emulator);
}

void SettingsStore::saveBrightness(uint8_t value)
{
    preferences_.putUChar("brightness", value);
}

uint8_t SettingsStore::loadBrightness() const
{
    uint8_t brightness =
        const_cast<Preferences &>(preferences_)
            .getUChar("brightness", 6);
    return brightness <= kBrightnessMax ? brightness : 6;
}

void SettingsStore::saveNightMode(const NightModeSettings &value)
{
    preferences_.putBool("night_enabled", value.enabled);
    preferences_.putUChar("night_start", value.start_hour);
    preferences_.putUChar("night_end", value.end_hour);
    preferences_.putBool("night_off", value.screen_off_enabled);
    preferences_.putUChar("night_off_at", value.screen_off_hour);
}

void SettingsStore::saveChime(const ChimeSettings &value,
                              const char *sound_path)
{
    preferences_.putUChar(
        "chime_mode", static_cast<uint8_t>(value.mode));
    preferences_.putUChar("chime_sound", value.sound);
    preferences_.putUChar("chime_volume", value.volume);
    preferences_.putBool("chime_quiet", value.quiet_enabled);
    preferences_.putUChar(
        "quiet_start", value.quiet_start_hour);
    preferences_.putUChar(
        "quiet_end", value.quiet_end_hour);
    if (sound_path)
        preferences_.putString("chime_path", sound_path);
}

String SettingsStore::loadChimePath(const char *fallback) const
{
    return const_cast<Preferences &>(preferences_)
        .getString("chime_path", fallback);
}

String SettingsStore::loadStartupSoundPath(
    const char *fallback) const
{
    return const_cast<Preferences &>(preferences_)
        .getString("startup_sound", fallback);
}

uint8_t SettingsStore::loadStartupSoundVolume(
    uint8_t fallback) const
{
    const uint8_t value =
        const_cast<Preferences &>(preferences_)
            .getUChar("startup_volume", fallback);
    return audio_volume_nearest_level(
        value <= 100 ? value : fallback);
}

String SettingsStore::loadFloppySoundPath(
    const char *fallback) const
{
    return const_cast<Preferences &>(preferences_)
        .getString("floppy_sound", fallback);
}

uint8_t SettingsStore::loadFloppySoundVolume(
    uint8_t fallback) const
{
    const uint8_t value =
        const_cast<Preferences &>(preferences_)
            .getUChar("floppy_volume", fallback);
    return audio_volume_nearest_level(
        value <= 100 ? value : fallback);
}

void SettingsStore::saveSystemSounds(
    const char *startup_path, uint8_t startup_volume,
    const char *floppy_path, uint8_t floppy_volume)
{
    if (startup_path)
        preferences_.putString("startup_sound", startup_path);
    preferences_.putUChar("startup_volume", startup_volume);
    if (floppy_path)
        preferences_.putString("floppy_sound", floppy_path);
    preferences_.putUChar("floppy_volume", floppy_volume);
}

Preferences &emulator_preferences()
{
    return g_settings_store->preferences();
}
