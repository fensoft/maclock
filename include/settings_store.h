#pragma once

#include <Preferences.h>

#include "app_types.h"

class SettingsStore
{
public:
    bool begin();
    AppSettings load();

    Preferences &preferences();

    void saveLanguage(UiLanguage value);
    void saveDateFormat(UiDateFormat value);
    void saveTemperatureUnit(UiTemperatureUnit value);
    void saveClockFace(ClockFace value);
    void saveClockTheme(ClockTheme value);
    void saveScreensaverMode(ScreensaverMode value);
    void saveScreensaverDelay(uint8_t value);
    void saveBootBrightness(BootBrightness value);
    void saveBootMode(bool emulator);
    void saveBrightness(uint8_t value);
    uint8_t loadBrightness() const;
    void saveNightMode(const NightModeSettings &value);
    void saveChime(const ChimeSettings &value, const char *sound_path);
    String loadChimePath(const char *fallback) const;

private:
    Preferences preferences_;
};

Preferences &emulator_preferences();
