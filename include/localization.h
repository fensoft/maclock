#pragma once

#include <stdint.h>

enum UiLanguage : uint8_t
{
    UI_LANGUAGE_ENGLISH,
    UI_LANGUAGE_FRENCH,
    UI_LANGUAGE_SPANISH,
    UI_LANGUAGE_GERMAN,
    UI_LANGUAGE_ITALIAN,
    UI_LANGUAGE_COUNT
};

void localization_set_language(UiLanguage language);
UiLanguage localization_get_language();
const char *localization_language_name(UiLanguage language);
const char *tr(const char *english);
