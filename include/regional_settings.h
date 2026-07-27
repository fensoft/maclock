#pragma once

#include <stdint.h>

enum UiDateFormat : uint8_t
{
    UI_DATE_FORMAT_DMY,
    UI_DATE_FORMAT_MDY,
    UI_DATE_FORMAT_YMD,
    UI_DATE_FORMAT_COUNT
};

enum UiTemperatureUnit : uint8_t
{
    UI_TEMPERATURE_CELSIUS,
    UI_TEMPERATURE_FAHRENHEIT,
    UI_TEMPERATURE_UNIT_COUNT
};
