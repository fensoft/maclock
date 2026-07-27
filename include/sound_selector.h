#pragma once

#include <stddef.h>
#include <stdint.h>

#include <lvgl.h>

static constexpr size_t SOUND_SELECTOR_MAX_FILES = 32;
static constexpr size_t SOUND_SELECTOR_PATH_MAX = 96;

using SoundSelectorChangedCallback =
    void (*)(const char *path, void *user_data);
using SoundSelectorPreviewCallback =
    bool (*)(const char *path, uint8_t volume);

class SoundSelector
{
public:
    static void scan();
    static void setPreviewCallback(
        SoundSelectorPreviewCallback callback);
    static const char *resolvePath(
        const char *path, const char *fallback_path);
    static const char *displayName(const char *path);

    void begin(
        lv_obj_t *parent,
        const char *selected_path,
        uint8_t preview_volume,
        SoundSelectorChangedCallback changed_callback,
        void *user_data);
    void setPath(const char *path);
    const char *path() const;
    void setPreviewVolume(uint8_t volume);
    void refreshLanguage();

    // LVGL callbacks use these fields through their instance user data.
    lv_obj_t *list = nullptr;
    lv_obj_t *play_button = nullptr;
    lv_obj_t *play_label = nullptr;
    lv_obj_t *empty_label = nullptr;
    lv_obj_t *items[SOUND_SELECTOR_MAX_FILES] = {};
    size_t selected = 0;
    uint8_t preview_volume = 0;
    SoundSelectorChangedCallback changed_callback = nullptr;
    void *user_data = nullptr;
};
