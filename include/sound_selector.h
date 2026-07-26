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

struct SoundSelector
{
    lv_obj_t *list;
    lv_obj_t *play_button;
    lv_obj_t *items[SOUND_SELECTOR_MAX_FILES];
    size_t selected;
    uint8_t preview_volume;
    SoundSelectorChangedCallback changed_callback;
    void *user_data;
};

void sound_selector_scan();
void sound_selector_set_preview_callback(
    SoundSelectorPreviewCallback callback);

void sound_selector_create(
    SoundSelector *selector,
    lv_obj_t *parent,
    const char *selected_path,
    uint8_t preview_volume,
    SoundSelectorChangedCallback changed_callback,
    void *user_data);

void sound_selector_set_path(
    SoundSelector *selector, const char *path);
const char *sound_selector_get_path(
    const SoundSelector *selector);
void sound_selector_set_preview_volume(
    SoundSelector *selector, uint8_t volume);

const char *sound_selector_resolve_path(
    const char *path, const char *fallback_path);
const char *sound_selector_display_name(const char *path);
