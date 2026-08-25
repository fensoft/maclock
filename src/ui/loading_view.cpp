#ifdef MACLOCK_COMBINED_SOURCE
namespace
{
constexpr size_t kLoadingMaxJsonBytes = 32768;

bool loading_color(const char *source, lv_color_t &color)
{
    if (!source || source[0] != '#' || strlen(source) != 7)
        return false;
    char *end = nullptr;
    const unsigned long value = strtoul(source + 1, &end, 16);
    if (!end || *end) return false;
    color = lv_color_hex(value);
    return true;
}

const lv_font_t *loading_font(const char *name)
{
    if (!name || !name[0] || !strcmp(name, "lv_font_chicago_8")) return &lv_font_chicago_8;
    if (!strcmp(name, "lv_font_chicago_24")) return &lv_font_chicago_24;
    if (!strcmp(name, "lv_font_chicago_32")) return &lv_font_chicago_32;
    if (!strcmp(name, "lv_font_chicago_48")) return &lv_font_chicago_48;
    return nullptr;
}

bool loading_asset_name(const char *name)
{
    return name && name[0] && !strchr(name, '/') && !strstr(name, "..");
}

bool loading_name_valid(const char *name)
{
    if (!name || !name[0]) return false;
    for (const char *cursor = name; *cursor; ++cursor)
        if (!((*cursor >= 'a' && *cursor <= 'z') ||
              (*cursor >= 'A' && *cursor <= 'Z') ||
              (*cursor >= '0' && *cursor <= '9') ||
              *cursor == '-' || *cursor == '_')) return false;
    return true;
}

bool loading_text(const char *source, JsonObjectConst translations, char *out, size_t out_size)
{
    if (!source || !out || !out_size) return false;
    const char *cursor = source;
    size_t length = 0;
    while (*cursor && length + 1 < out_size)
    {
        if (strncmp(cursor, "{tr.", 4)) { out[length++] = *cursor++; continue; }
        const char *end = strchr(cursor, '}');
        if (!end || end - cursor <= 4) return false;
        char key[48] = {};
        const size_t key_size = end - cursor - 4;
        if (key_size >= sizeof(key)) return false;
        memcpy(key, cursor + 4, key_size);
        const char *language = "en";
        switch (app_settings.language)
        {
        case UI_LANGUAGE_FRENCH: language = "fr"; break;
        case UI_LANGUAGE_SPANISH: language = "es"; break;
        case UI_LANGUAGE_GERMAN: language = "de"; break;
        case UI_LANGUAGE_ITALIAN: language = "it"; break;
        default: break;
        }
        JsonObjectConst entry = translations[key];
        const char *translated = entry[language] | "";
        if (!translated[0]) translated = entry["en"] | "";
        const size_t translated_size = strlen(translated);
        if (length + translated_size >= out_size) return false;
        memcpy(out + length, translated, translated_size);
        length += translated_size;
        cursor = end + 1;
    }
    if (*cursor) return false;
    out[length] = '\0';
    return true;
}

bool loading_project_path(const char *name, String &path)
{
    if (!loading_name_valid(name)) return false;
    path = String("/loading/") + name + "/loading.json";
    return LittleFS.exists(path.c_str());
}
} // namespace

void LoadingView::clear()
{
    if (root) { lv_obj_delete(root); root = nullptr; }
    for (lv_draw_buf_t *&buffer : buffers)
    {
        if (buffer) lv_draw_buf_destroy(buffer);
        buffer = nullptr;
    }
    memset(modules, 0, sizeof(modules));
    module_count = module_reveal = 0;
    sound_path[0] = '\0';
    sound_volume = 0;
}

void LoadingView::setPreviewScreen(const char *screen)
{
    strlcpy(preview_screen, screen ? screen : "", sizeof(preview_screen));
}

bool LoadingView::begin(uint32_t now)
{
    clear();
    String selected = preview_screen[0] ? preview_screen : app_settings.loading_screen;
    preview_screen[0] = '\0';
    File directory;
    if (!selected.length()) directory = LittleFS.open("/loading");

    for (;;)
    {
        if (!selected.length())
        {
            if (!directory || !directory.isDirectory()) return false;
            File entry = directory.openNextFile();
            if (!entry) { directory.close(); return false; }
            if (!entry.isDirectory()) continue;
            const String entry_path = entry.name();
            selected = entry_path.substring(entry_path.lastIndexOf("/") + 1);
        }

        String project_path;
        if (!loading_project_path(selected.c_str(), project_path))
        {
            if (app_settings.loading_screen[0]) { selected = ""; directory = LittleFS.open("/loading"); continue; }
            selected = "";
            continue;
        }
        File file = LittleFS.open(project_path.c_str(), "r");
        JsonDocument document;
        if (!file || file.size() > kLoadingMaxJsonBytes || deserializeJson(document, file))
        {
            if (file) file.close();
            if (app_settings.loading_screen[0]) { selected = ""; directory = LittleFS.open("/loading"); continue; }
            selected = "";
            continue;
        }
        file.close();
        JsonArrayConst objects = document["objects"];
        if (strcmp(document["format"] | "", "maclock-loading-screen") ||
            document["version"] != 1 || document["width"] != 304 ||
            document["height"] != 224 || objects.isNull() ||
            objects.size() > kMaxObjects)
        {
            if (app_settings.loading_screen[0]) { selected = ""; directory = LittleFS.open("/loading"); continue; }
            selected = "";
            continue;
        }

        lv_color_t background;
        if (!loading_color(document["background"] | "#000000", background)) background = lv_color_black();
        root = create_clock_face_root(lv_screen_active(), background);
        JsonObjectConst translations = document["translations"];
        const ClockRenderSnapshot visibility_snapshot =
            make_clock_snapshot(now);
        bool valid = true;
        size_t object_index = 0;
        for (JsonObjectConst item : objects)
        {
            if (!custom_face_visible(
                    item["visible_if"] | "",
                    visibility_snapshot))
                continue;
            const char *type = item["type"] | "";
            const int16_t x = item["x"] | 0, y = item["y"] | 0;
            const int16_t width = item["width"] | 0, height = item["height"] | 0;
            if (width < 0 || height < 0) { valid = false; break; }
            lv_color_t stroke;
            if (!loading_color(item["stroke"] | "#000000", stroke)) stroke = lv_color_black();
            if (!strcmp(type, "rectangle") || !strcmp(type, "circle"))
            {
                lv_obj_t *object = lv_obj_create(root);
                lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_set_pos(object, x, y); lv_obj_set_size(object, width, height);
                lv_obj_set_style_border_color(object, stroke, 0);
                lv_obj_set_style_border_width(object, item["stroke_width"] | 1, 0);
                lv_color_t fill;
                if (loading_color(item["fill"] | "", fill)) { lv_obj_set_style_bg_color(object, fill, 0); lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0); }
                else lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, 0);
                if (!strcmp(type, "circle")) lv_obj_set_style_radius(object, LV_RADIUS_CIRCLE, 0);
                else lv_obj_set_style_radius(object, item["border_radius"] | 0, 0);
            }
            else if (!strcmp(type, "line"))
            {
                lv_obj_t *object = lv_line_create(root);
                line_points[object_index][0] = {0, 0}; line_points[object_index][1] = {width, height};
                lv_line_set_points(object, line_points[object_index], 2);
                lv_obj_set_pos(object, x, y);
                lv_obj_set_style_line_color(object, stroke, 0);
                lv_obj_set_style_line_width(object, item["stroke_width"] | 1, 0);
            }
            else if (!strcmp(type, "text"))
            {
                const lv_font_t *font = loading_font(item["font_family"] | "lv_font_chicago_8");
                char text[160];
                if (!font || !loading_text(item["template"] | "", translations, text, sizeof(text))) { valid = false; break; }
                lv_obj_t *object = create_clock_face_label(root, font, stroke);
                lv_label_set_text(object, text); lv_obj_set_pos(object, x, y); lv_obj_set_width(object, width);
                const char *align = item["align"] | "left";
                lv_obj_set_style_text_align(object, !strcmp(align, "right") ? LV_TEXT_ALIGN_RIGHT : !strcmp(align, "center") ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT, 0);
            }
            else if (!strcmp(type, "image"))
            {
                const bool module = !strcmp(item["id"] | "", "module");
                const char *template_name = item["template"] | item["source"] | "";
                if (module && !strcmp(template_name, "plugin_{i2c}.png"))
                {
                    if (module_count || object_index + kMaxModules > kMaxObjects)
                    { valid = false; break; }
                    const uint8_t addresses[kMaxModules] = {0x18, 0x38, weather_service.address(), 0x68};
                    const int16_t step_x = item["next_module_x"] | 0, step_y = item["next_module_y"] | 0;
                    for (size_t index = 0; index < kMaxModules; ++index)
                    {
                        char asset[24]; snprintf(asset, sizeof(asset), "plugin_0x%02X.png", addresses[index]);
                        String image_path = String("/loading/") + selected + "/" + asset;
                        lv_draw_buf_t *buffer = LittleFS.exists(image_path.c_str()) ? load_png_once((String("S:") + image_path).c_str()) : nullptr;
                        if (!buffer) buffer = lv_draw_buf_dup(ui_shell.plugin_buf);
                        if (!buffer) { valid = false; break; }
                        if (!addresses[index] || !i2c_bus.present(addresses[index])) replace_black_with_red(buffer);
                        buffers[object_index++] = buffer;
                        lv_obj_t *object = lv_image_create(root);
                        modules[module_count++] = object;
                        lv_obj_set_pos(object, x + index * step_x, y + index * step_y); lv_obj_set_size(object, width, height);
                        set_image_src(object, buffer, nullptr);
                        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
                    }
                    if (!valid) break;
                }
                else
                {
                    if (!loading_asset_name(template_name)) { valid = false; break; }
                    const String image_path = String("/loading/") + selected + "/" + template_name;
                    if (!LittleFS.exists(image_path.c_str())) { valid = false; break; }
                    lv_draw_buf_t *buffer = load_png_once((String("S:") + image_path).c_str());
                    if (!buffer) { valid = false; break; }
                    buffers[object_index] = buffer;
                    lv_obj_t *object = lv_image_create(root);
                    lv_obj_set_pos(object, x, y); lv_obj_set_size(object, width, height);
                    set_image_src(object, buffer, nullptr);
                }
            }
            else { valid = false; break; }
            ++object_index;
        }
        if (!valid) { clear(); if (app_settings.loading_screen[0]) { selected = ""; directory = LittleFS.open("/loading"); continue; } selected = ""; continue; }
        const char *sound = document["sound"] | "";
        const uint8_t volume = document["sound_volume"] | g_floppy_sound_volume;
        if (sound[0] == '/' && !strstr(sound, "..") && LittleFS.exists(sound) &&
            audio_volume_is_level(volume))
        {
            strlcpy(sound_path, sound, sizeof(sound_path));
            sound_volume = volume;
        }
        started_ms = now; next_reveal_ms = now + random(100, 301);
        return true;
    }
}

void LoadingView::show(uint32_t now)
{
    if (!root) return;
    lv_obj_clear_flag(root, LV_OBJ_FLAG_HIDDEN);
    if (module_reveal < module_count && now >= next_reveal_ms)
    {
        lv_obj_clear_flag(modules[module_reveal++], LV_OBJ_FLAG_HIDDEN);
        next_reveal_ms = now + random(200, 601);
    }
}

bool LoadingView::finished(uint32_t now) const
{
    return root && now - started_ms >= 1500 && module_reveal == module_count;
}
#endif
