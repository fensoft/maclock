#include "sound_selector.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>

LV_FONT_DECLARE(lv_font_chicago_8);

namespace
{
static char g_paths[SOUND_SELECTOR_MAX_FILES]
                   [SOUND_SELECTOR_PATH_MAX] = {};
static size_t g_path_count = 0;
static bool g_scanned = false;
static SoundSelectorPreviewCallback g_preview_callback = nullptr;
static constexpr uint8_t kPreviewVolume = 80;

static bool is_mp3_path(const char *path)
{
    if (!path)
        return false;
    const size_t length = strlen(path);
    return length >= 4 &&
           strcasecmp(path + length - 4, ".mp3") == 0;
}

static void add_path(const char *path)
{
    if (!path || !path[0] ||
        g_path_count >= SOUND_SELECTOR_MAX_FILES)
    {
        return;
    }

    char normalized[SOUND_SELECTOR_PATH_MAX];
    if (path[0] == '/')
        strlcpy(normalized, path, sizeof(normalized));
    else
        snprintf(normalized, sizeof(normalized), "/%s", path);

    for (size_t i = 0; i < g_path_count; ++i)
    {
        if (strcasecmp(g_paths[i], normalized) == 0)
            return;
    }
    strlcpy(
        g_paths[g_path_count++], normalized,
        SOUND_SELECTOR_PATH_MAX);
}

static void scan_directory(fs::File &directory)
{
    fs::File entry = directory.openNextFile();
    while (entry)
    {
        if (entry.isDirectory())
        {
            scan_directory(entry);
        }
        else if (is_mp3_path(entry.path()))
        {
            add_path(entry.path());
        }
        entry.close();
        entry = directory.openNextFile();
    }
}

static void sort_paths()
{
    for (size_t i = 1; i < g_path_count; ++i)
    {
        char current[SOUND_SELECTOR_PATH_MAX];
        strlcpy(current, g_paths[i], sizeof(current));
        size_t destination = i;
        while (destination > 0 &&
               strcasecmp(
                   g_paths[destination - 1], current) > 0)
        {
            strlcpy(
                g_paths[destination],
                g_paths[destination - 1],
                SOUND_SELECTOR_PATH_MAX);
            --destination;
        }
        strlcpy(
            g_paths[destination], current,
            SOUND_SELECTOR_PATH_MAX);
    }
}

static size_t find_path(const char *path)
{
    if (!path || !path[0])
        return SIZE_MAX;
    for (size_t i = 0; i < g_path_count; ++i)
    {
        if (strcasecmp(g_paths[i], path) == 0)
            return i;
    }
    return SIZE_MAX;
}

static const char *relative_name(const char *path)
{
    if (!path)
        return "";
    return path[0] == '/' ? path + 1 : path;
}

static void format_display_name(
    const char *path, char *name, size_t name_size)
{
    if (!name || name_size == 0)
        return;

    strlcpy(name, relative_name(path), name_size);
    const size_t length = strlen(name);
    if (length >= 4 &&
        strcasecmp(name + length - 4, ".mp3") == 0)
    {
        name[length - 4] = '\0';
    }
}

static void update_selection_visuals(
    SoundSelector *selector, bool scroll_to_selected)
{
    if (!selector)
        return;

    for (size_t i = 0; i < g_path_count; ++i)
    {
        if (!selector->items[i])
            continue;
        if (i == selector->selected)
            lv_obj_add_state(
                selector->items[i], LV_STATE_CHECKED);
        else
            lv_obj_remove_state(
                selector->items[i], LV_STATE_CHECKED);
    }

    if (scroll_to_selected &&
        selector->selected < g_path_count &&
        selector->items[selector->selected])
    {
        lv_obj_scroll_to_view(
            selector->items[selector->selected], LV_ANIM_OFF);
    }
}

static void selection_event(lv_event_t *event)
{
    SoundSelector *selector =
        (SoundSelector *)lv_event_get_user_data(event);
    lv_obj_t *item = (lv_obj_t *)lv_event_get_target(event);
    if (!selector || !item)
        return;

    const size_t selected =
        (size_t)(uintptr_t)lv_obj_get_user_data(item);
    if (selected >= g_path_count)
        return;

    selector->selected = selected;
    update_selection_visuals(selector, false);
    if (selector->changed_callback)
    {
        selector->changed_callback(
            g_paths[selected], selector->user_data);
    }
}

static void play_event(lv_event_t *event)
{
    SoundSelector *selector =
        (SoundSelector *)lv_event_get_user_data(event);
    if (!selector || !g_preview_callback ||
        selector->selected >= g_path_count)
    {
        return;
    }
    g_preview_callback(
        g_paths[selector->selected],
        kPreviewVolume);
}

static void style_list_item(lv_obj_t *item)
{
    const lv_style_selector_t checked =
        (lv_style_selector_t)LV_STATE_CHECKED;
    const lv_style_selector_t pressed =
        (lv_style_selector_t)LV_STATE_PRESSED;

    lv_obj_add_flag(item, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_width(item, lv_pct(100));
    lv_obj_set_height(item, 38);
    lv_obj_set_style_bg_color(item, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(item, lv_color_black(), 0);
    lv_obj_set_style_text_font(item, &lv_font_chicago_8, 0);
    lv_obj_set_style_border_color(item, lv_color_black(), 0);
    lv_obj_set_style_border_width(item, 1, 0);
    lv_obj_set_style_radius(item, 4, 0);
    lv_obj_set_style_shadow_width(item, 0, 0);
    lv_obj_set_style_outline_width(item, 0, 0);
    lv_obj_set_style_bg_color(item, lv_color_black(), checked);
    lv_obj_set_style_text_color(item, lv_color_white(), checked);
    lv_obj_set_style_bg_color(item, lv_color_black(), pressed);
    lv_obj_set_style_text_color(item, lv_color_white(), pressed);
}

static lv_obj_t *create_play_button(
    lv_obj_t *parent, SoundSelector *selector)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, 260, 40);
    lv_obj_align(button, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(button, lv_color_black(), 0);
    lv_obj_set_style_border_color(button, lv_color_black(), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_radius(button, 4, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_outline_width(button, 0, 0);
    lv_obj_set_style_bg_color(
        button, lv_color_black(), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(
        button, lv_color_white(), LV_STATE_PRESSED);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, "Play");
    lv_obj_set_style_text_font(label, &lv_font_chicago_8, 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(
        button, play_event, LV_EVENT_CLICKED, selector);
    return button;
}
}

void sound_selector_scan()
{
    g_path_count = 0;
    fs::File root = LittleFS.open("/");
    if (root && root.isDirectory())
        scan_directory(root);
    root.close();
    sort_paths();
    g_scanned = true;
    Serial.printf(
        "Sound selector: %u MP3 file(s) in LittleFS\n",
        (unsigned)g_path_count);
}

void sound_selector_set_preview_callback(
    SoundSelectorPreviewCallback callback)
{
    g_preview_callback = callback;
}

void sound_selector_create(
    SoundSelector *selector,
    lv_obj_t *parent,
    const char *selected_path,
    uint8_t preview_volume,
    SoundSelectorChangedCallback changed_callback,
    void *user_data)
{
    if (!selector || !parent)
        return;
    if (!g_scanned)
        sound_selector_scan();

    memset(selector, 0, sizeof(*selector));
    selector->preview_volume = preview_volume;
    selector->changed_callback = changed_callback;
    selector->user_data = user_data;

    selector->list = lv_list_create(parent);
    lv_obj_set_size(selector->list, 260, 78);
    lv_obj_align(selector->list, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(
        selector->list, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        selector->list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(selector->list, 0, 0);
    lv_obj_set_style_radius(selector->list, 0, 0);
    lv_obj_set_style_pad_all(selector->list, 0, 0);
    lv_obj_set_style_pad_row(selector->list, 4, 0);
    lv_obj_set_scrollbar_mode(
        selector->list, LV_SCROLLBAR_MODE_AUTO);

    for (size_t i = 0; i < g_path_count; ++i)
    {
        char display_name[SOUND_SELECTOR_PATH_MAX];
        format_display_name(
            g_paths[i], display_name, sizeof(display_name));
        lv_obj_t *item = lv_list_add_button(
            selector->list, nullptr, display_name);
        selector->items[i] = item;
        style_list_item(item);
        lv_obj_set_user_data(item, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(
            item, selection_event, LV_EVENT_CLICKED, selector);
    }

    if (g_path_count == 0)
    {
        lv_obj_t *empty = lv_label_create(selector->list);
        lv_label_set_text(empty, "No MP3 files in LittleFS");
        lv_obj_set_style_text_font(
            empty, &lv_font_chicago_8, 0);
        lv_obj_center(empty);
    }

    selector->play_button =
        create_play_button(parent, selector);
    if (g_path_count == 0)
        lv_obj_add_state(
            selector->play_button, LV_STATE_DISABLED);

    sound_selector_set_path(selector, selected_path);
}

void sound_selector_set_path(
    SoundSelector *selector, const char *path)
{
    if (!selector)
        return;

    size_t selected = find_path(path);
    if (selected == SIZE_MAX)
        selected = find_path("/quack.mp3");
    if (selected == SIZE_MAX && g_path_count > 0)
        selected = 0;

    selector->selected = selected;
    update_selection_visuals(selector, true);
}

const char *sound_selector_get_path(
    const SoundSelector *selector)
{
    if (!selector || selector->selected >= g_path_count)
        return nullptr;
    return g_paths[selector->selected];
}

void sound_selector_set_preview_volume(
    SoundSelector *selector, uint8_t volume)
{
    if (selector)
        selector->preview_volume = volume;
}

const char *sound_selector_resolve_path(
    const char *path, const char *fallback_path)
{
    if (!g_scanned)
        sound_selector_scan();

    size_t selected = find_path(path);
    if (selected != SIZE_MAX)
        return g_paths[selected];
    selected = find_path(fallback_path);
    if (selected != SIZE_MAX)
        return g_paths[selected];
    return g_path_count > 0 ? g_paths[0] : fallback_path;
}

const char *sound_selector_display_name(const char *path)
{
    static char display_name[SOUND_SELECTOR_PATH_MAX];
    format_display_name(
        sound_selector_resolve_path(path, "/quack.mp3"),
        display_name, sizeof(display_name));
    return display_name;
}
