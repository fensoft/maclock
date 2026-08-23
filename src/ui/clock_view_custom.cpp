#ifdef MACLOCK_COMBINED_SOURCE
namespace
{
constexpr size_t kCustomFaceMaxObjects = 64;
constexpr size_t kCustomFaceMaxJsonBytes = 32768;

static void flip_scale_y_animation(void *object, int32_t scale)
{
    lv_obj_set_style_transform_scale_y(
        static_cast<lv_obj_t *>(object), scale, 0);
}

static uint32_t custom_flip_collapse_duration()
{
    return g_face_customization.flip_speed == FlipAnimationSpeed::Slow ? 260 :
        g_face_customization.flip_speed == FlipAnimationSpeed::Fast ? 100 : 170;
}

static uint32_t custom_flip_expand_duration()
{
    return g_face_customization.flip_speed == FlipAnimationSpeed::Slow ? 320 :
        g_face_customization.flip_speed == FlipAnimationSpeed::Fast ? 130 : 210;
}

static uint32_t custom_odometer_duration()
{
    return g_face_customization.flip_speed == FlipAnimationSpeed::Slow ? 480 :
        g_face_customization.flip_speed == FlipAnimationSpeed::Fast ? 160 : 320;
}

static uint32_t custom_colon_blink_period()
{
    return g_face_customization.colon_blink == ColonBlinkInterval::HalfSecond
        ? 500 : 1000;
}

static uint32_t custom_second_refresh_interval();
static uint32_t custom_elapsed_in_second(const ClockRenderSnapshot &snapshot);

static bool custom_colon_visible(const ClockRenderSnapshot &snapshot)
{
    if (g_face_customization.colon_blink == ColonBlinkInterval::None)
        return true;
    if (g_face_customization.colon_blink == ColonBlinkInterval::OneSecond)
        return snapshot.current.unixtime() % 2;
    return (custom_elapsed_in_second(snapshot) /
        custom_colon_blink_period()) % 2;
}

static uint32_t custom_face_refresh_interval()
{
    if (g_face_customization.colon_blink == ColonBlinkInterval::None)
        return custom_second_refresh_interval();
    const uint32_t blink = custom_colon_blink_period();
    const uint32_t seconds = custom_second_refresh_interval();
    return blink < seconds ? blink : seconds;
}

static int16_t custom_text_width(const lv_font_t *font, const char *text)
{
    int16_t width = 0;
    for (const char *cursor = text; *cursor; ++cursor)
        width += lv_font_get_glyph_width(font,
            static_cast<uint8_t>(*cursor),
            static_cast<uint8_t>(cursor[1]));
    return width;
}

static uint32_t custom_second_refresh_interval()
{
    return g_face_customization.continuous_seconds ? 20 : 1000;
}

static uint32_t custom_elapsed_in_second(const ClockRenderSnapshot &snapshot)
{
    static uint32_t observed_epoch = UINT32_MAX;
    static uint32_t second_started_ms = 0;
    const uint32_t now_ms = millis();
    const uint32_t epoch = snapshot.current.unixtime();
    if (epoch != observed_epoch)
    {
        observed_epoch = epoch;
        second_started_ms = now_ms;
    }
    const uint32_t raw_elapsed = now_ms - second_started_ms;
    return raw_elapsed > 999 ? 999 : raw_elapsed;
}

static float custom_precise_second(const ClockRenderSnapshot &snapshot)
{
    const uint32_t interval = custom_second_refresh_interval();
    const uint32_t elapsed = custom_elapsed_in_second(snapshot);
    const float fraction = interval >= 1000 ? 0.0f :
        (elapsed / interval) * interval / 1000.0f;
    return snapshot.current.second() + fraction;
}

static void custom_flip_expand_completed(lv_anim_t *animation)
{
    FlipCardAnimation *state = static_cast<FlipCardAnimation *>(
        lv_anim_get_user_data(animation));
    if (!state) return;
    strlcpy(state->displayed, state->pending, sizeof(state->displayed));
    lv_label_set_text(state->top_label, state->displayed);
    lv_label_set_text(state->bottom_label, state->displayed);
    lv_obj_set_style_transform_scale_y(state->bottom_flap, LV_SCALE_NONE, 0);
    lv_obj_add_flag(state->bottom_flap, LV_OBJ_FLAG_HIDDEN);
    state->animating = false;
}

static void custom_flip_collapse_completed(lv_anim_t *animation)
{
    FlipCardAnimation *state = static_cast<FlipCardAnimation *>(
        lv_anim_get_user_data(animation));
    if (!state) return;
    lv_obj_add_flag(state->top_flap, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(state->bottom_flap_label, state->pending);
    lv_obj_clear_flag(state->bottom_flap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(state->bottom_flap);
    lv_anim_t expand;
    lv_anim_init(&expand);
    lv_anim_set_var(&expand, state->bottom_flap);
    lv_anim_set_exec_cb(&expand, flip_scale_y_animation);
    lv_anim_set_values(&expand, 12, LV_SCALE_NONE);
    lv_anim_set_duration(&expand, custom_flip_expand_duration());
    lv_anim_set_path_cb(&expand, lv_anim_path_ease_out);
    lv_anim_set_user_data(&expand, state);
    lv_anim_set_completed_cb(&expand, custom_flip_expand_completed);
    lv_anim_start(&expand);
}

static void update_flip_card(FlipCardAnimation &state, const char *value)
{
    if (!state.initialized)
    {
        strlcpy(state.displayed, value, sizeof(state.displayed));
        strlcpy(state.pending, value, sizeof(state.pending));
        for (lv_obj_t *label : {state.top_label, state.bottom_label,
             state.top_flap_label, state.bottom_flap_label})
            lv_label_set_text(label, value);
        state.initialized = true;
        return;
    }
    if (state.animating || !strcmp(value, state.displayed))
        return;
    strlcpy(state.pending, value, sizeof(state.pending));
    lv_label_set_text(state.top_label, state.pending);
    lv_label_set_text(state.bottom_label, state.displayed);
    lv_label_set_text(state.top_flap_label, state.displayed);
    lv_label_set_text(state.bottom_flap_label, state.pending);
    lv_obj_set_style_transform_scale_y(state.top_flap, LV_SCALE_NONE, 0);
    lv_obj_set_style_transform_scale_y(state.bottom_flap, 12, 0);
    lv_obj_clear_flag(state.top_flap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(state.bottom_flap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(state.top_flap);
    state.animating = true;
    lv_anim_t collapse;
    lv_anim_init(&collapse);
    lv_anim_set_var(&collapse, state.top_flap);
    lv_anim_set_exec_cb(&collapse, flip_scale_y_animation);
    lv_anim_set_values(&collapse, LV_SCALE_NONE, 12);
    lv_anim_set_duration(&collapse, custom_flip_collapse_duration());
    lv_anim_set_path_cb(&collapse, lv_anim_path_ease_in);
    lv_anim_set_user_data(&collapse, &state);
    lv_anim_set_completed_cb(&collapse, custom_flip_collapse_completed);
    lv_anim_start(&collapse);
}

static bool custom_face_name_valid(const char *name)
{
    if (!name || !name[0])
        return false;
    for (const char *cursor = name; *cursor; ++cursor)
    {
        const char value = *cursor;
        if (!((value >= 'a' && value <= 'z') ||
              (value >= '0' && value <= '9') ||
              value == '_' || value == '-'))
            return false;
    }
    return true;
}

static const lv_font_t *custom_face_font(const char *name)
{
    if (!name || !name[0] || !strcmp(name, "lv_font_chicago_8"))
        return &lv_font_chicago_8;
    if (!strcmp(name, "lv_font_chicago_24")) return &lv_font_chicago_24;
    if (!strcmp(name, "lv_font_chicago_32")) return &lv_font_chicago_32;
    if (!strcmp(name, "lv_font_chicago_48")) return &lv_font_chicago_48;
    if (!strcmp(name, "lv_font_chicago_digits_6")) return &lv_font_chicago_digits_6;
    if (!strcmp(name, "lv_font_chicago_digits_10")) return &lv_font_chicago_digits_10;
    if (!strcmp(name, "lv_font_chicago_digits_40")) return &lv_font_chicago_digits_40;
    if (!strcmp(name, "lv_font_chicago_digits_56")) return &lv_font_chicago_digits_56;
    if (!strcmp(name, "lv_font_seven_segment_24")) return &lv_font_seven_segment_24;
    if (!strcmp(name, "lv_font_seven_segment_48")) return &lv_font_seven_segment_48;
    if (!strcmp(name, "lv_font_seven_segment_64")) return &lv_font_seven_segment_64;
    if (!strcmp(name, "lv_font_seven_segment_80")) return &lv_font_seven_segment_80;
    if (!strcmp(name, "lv_font_seven_segment_96")) return &lv_font_seven_segment_96;
    return nullptr;
}

static bool custom_face_color(const char *source, lv_color_t &color)
{
    if (!source || source[0] != '#')
        return false;
    if (strlen(source) == 4)
    {
        const char digits[] = {source[1], source[1], source[2], source[2], source[3], source[3], '\0'};
        char *end = nullptr;
        const unsigned long value = strtoul(digits, &end, 16);
        if (!end || *end)
            return false;
        color = lv_color_hex(value);
        return true;
    }
    char *end = nullptr;
    const unsigned long value = strtoul(source + 1, &end, 16);
    if (!end || *end || strlen(source) != 7)
        return false;
    color = lv_color_hex(value);
    return true;
}

static void custom_face_error(ClockView &view, const char *message)
{
    if (view.custom_face)
        lv_obj_clean(view.custom_face);
    lv_obj_set_style_bg_color(view.custom_face, lv_color_black(), 0);
    view.custom_error = create_clock_face_label(
        view.custom_face, &lv_font_chicago_8, lv_color_white());
    lv_obj_set_width(view.custom_error, 280);
    lv_obj_set_style_text_align(view.custom_error, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text_fmt(
        view.custom_error, "CLOCK FACE ERROR\n%s", message);
    lv_obj_center(view.custom_error);
    view.custom_loaded = false;
}

static lv_obj_t *create_custom_flip_half(
    lv_obj_t *card, int16_t y, int16_t width, int16_t height,
    bool bottom, lv_obj_t **label)
{
    lv_obj_t *half = lv_obj_create(card);
    lv_obj_remove_style_all(half);
    lv_obj_remove_flag(half, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(half, 2, y);
    lv_obj_set_size(half, width - 4, height);
    lv_obj_set_style_bg_color(half,
        bottom ? lv_color_hex(0x101010) : lv_color_hex(0x1d1d1d), 0);
    lv_obj_set_style_bg_opa(half, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(half, lv_color_hex(0x090909), 0);
    lv_obj_set_style_border_width(half, 1, 0);
    lv_obj_set_style_radius(half, 3, 0);
    *label = create_clock_face_label(half, &lv_font_chicago_48, lv_color_white());
    lv_obj_set_width(*label, lv_pct(100));
    return half;
}

static lv_obj_t *create_custom_flip_flap(
    lv_obj_t *card, int16_t y, int16_t width, int16_t height,
    bool bottom, lv_obj_t **label)
{
    lv_obj_t *flap = create_custom_flip_half(
        card, y, width, height, bottom, label);
    lv_obj_set_style_transform_pivot_x(flap, (width - 4) / 2, 0);
    lv_obj_set_style_transform_pivot_y(flap, bottom ? 0 : height, 0);
    lv_obj_set_style_transform_scale_y(flap, LV_SCALE_NONE, 0);
    lv_obj_add_flag(flap, LV_OBJ_FLAG_HIDDEN);
    return flap;
}

static void create_custom_flip_card(
    CustomFaceWidget &widget, uint8_t index, int16_t x, int16_t width)
{
    FlipCardAnimation &state = widget.flip_cards[index];
    const int16_t half_height = (widget.height - 6) / 2;
    const int16_t bottom_y = 4 + half_height;
    state.card = lv_obj_create(widget.root);
    lv_obj_remove_style_all(state.card);
    lv_obj_remove_flag(state.card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(state.card, x, 0);
    lv_obj_set_size(state.card, width, widget.height);
    lv_obj_set_style_bg_color(state.card, lv_color_hex(0x55544e), 0);
    lv_obj_set_style_bg_opa(state.card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(state.card, lv_color_hex(0x77766e), 0);
    lv_obj_set_style_border_width(state.card, 1, 0);
    lv_obj_set_style_radius(state.card, 6, 0);
    state.top_panel = create_custom_flip_half(
        state.card, 2, width, half_height, false, &state.top_label);
    state.bottom_panel = create_custom_flip_half(
        state.card, bottom_y, width, half_height, true, &state.bottom_label);
    state.top_flap = create_custom_flip_flap(
        state.card, 2, width, half_height, false, &state.top_flap_label);
    state.bottom_flap = create_custom_flip_flap(
        state.card, bottom_y, width, half_height, true, &state.bottom_flap_label);
    const int16_t label_y = (widget.height -
        static_cast<int16_t>(lv_font_get_line_height(&lv_font_chicago_48))) / 2 - 4;
    lv_obj_set_y(state.top_label, label_y);
    lv_obj_set_y(state.top_flap_label, label_y);
    lv_obj_set_y(state.bottom_label, label_y - bottom_y);
    lv_obj_set_y(state.bottom_flap_label, label_y - bottom_y);
}

static void custom_widget_set_text(
    CustomFaceWidget &widget, const char *text, bool initial)
{
    if (!initial && !strcmp(widget.displayed, text))
        return;
    if (widget.type == CustomFaceWidgetType::Flip)
    {
        if (initial)
        {
            widget.character_count = min(strlen(text),
                sizeof(widget.flip_cards) / sizeof(widget.flip_cards[0]));
            const int16_t cell_width = widget.character_count
                ? widget.width / widget.character_count : widget.width;
            for (uint8_t i = 0; i < widget.character_count; ++i)
                create_custom_flip_card(widget, i, i * cell_width, cell_width);
        }
        for (uint8_t i = 0; i < widget.character_count && text[i]; ++i)
        {
            char digit[2] = {text[i], '\0'};
            update_flip_card(widget.flip_cards[i], digit);
        }
        strlcpy(widget.displayed, text, sizeof(widget.displayed));
        return;
    }
    if (initial)
    {
        widget.character_count = min(
            strlen(text), sizeof(widget.current) / sizeof(widget.current[0]));
        const int16_t cell_width =
            widget.character_count ? widget.width / widget.character_count : widget.width;
        widget.text_y = (widget.height -
            static_cast<int16_t>(lv_font_get_line_height(widget.font))) / 2 - 4 +
            (widget.type == CustomFaceWidgetType::Odometer ? 2 : 0);
        for (uint8_t i = 0; i < widget.character_count; ++i)
        {
            char digit[2] = {text[i], '\0'};
            int16_t label_x = i * cell_width;
            int16_t label_width = cell_width;
            lv_obj_t *label_parent = widget.root;
            if (widget.type == CustomFaceWidgetType::Odometer &&
                isdigit(static_cast<unsigned char>(text[i])))
            {
                const int16_t window_width = cell_width - 2;
                const int16_t window_x =
                    label_x + (cell_width - window_width) / 2;
                lv_obj_t *window = lv_obj_create(widget.root);
                lv_obj_remove_style_all(window);
                lv_obj_remove_flag(window, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_clear_flag(window, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
                lv_obj_set_size(window, window_width, widget.height);
                lv_obj_set_pos(window, window_x, 0);
                lv_obj_set_style_border_color(window, widget.stroke, 0);
                lv_obj_set_style_border_width(window, 1, 0);
                lv_obj_set_style_radius(window, 2, 0);
                label_parent = window;
                label_x = 0;
                label_width = window_width;
            }
            widget.current[i] = create_clock_face_label(
                label_parent, widget.font, lv_color_white());
            widget.next[i] = create_clock_face_label(
                label_parent, widget.font, lv_color_white());
            for (lv_obj_t *label : {widget.current[i], widget.next[i]})
            {
                lv_obj_set_pos(label, label_x, widget.text_y);
                lv_obj_set_size(label, label_width, widget.height);
                lv_obj_set_style_text_align(label, widget.align, 0);
                lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
            }
            lv_label_set_text(widget.current[i], digit);
            lv_obj_set_y(widget.next[i], widget.text_y + widget.height);
        }
        strlcpy(widget.displayed, text, sizeof(widget.displayed));
        return;
    }
    for (uint8_t i = 0; i < widget.character_count && text[i]; ++i)
    {
        if (widget.displayed[i] == text[i])
            continue;
        char digit[2] = {text[i], '\0'};
        lv_anim_delete(widget.current[i], nullptr);
        lv_anim_delete(widget.next[i], nullptr);
        lv_label_set_text(widget.next[i], digit);
        lv_obj_set_y(widget.next[i], widget.text_y + widget.height);
        for (lv_obj_t *label : {widget.current[i], widget.next[i]})
        {
            lv_anim_t animation;
            lv_anim_init(&animation);
            lv_anim_set_var(&animation, label);
            lv_anim_set_values(&animation,
                label == widget.current[i] ? widget.text_y : widget.text_y + widget.height,
                label == widget.current[i] ? widget.text_y - widget.height : widget.text_y);
            lv_anim_set_duration(&animation,
                widget.type == CustomFaceWidgetType::Flip ? 180 :
                custom_odometer_duration());
            lv_anim_set_exec_cb(&animation, [](void *object, int32_t value) {
                lv_obj_set_y(static_cast<lv_obj_t *>(object), value);
            });
            lv_anim_start(&animation);
        }
        lv_obj_t *swap = widget.current[i];
        widget.current[i] = widget.next[i];
        widget.next[i] = swap;
    }
    strlcpy(widget.displayed, text, sizeof(widget.displayed));
}

static bool custom_face_expand(
    const char *source, const ClockRenderSnapshot &snapshot,
    JsonObjectConst translations, char *destination,
    size_t destination_size)
{
    if (!source || !destination || !destination_size)
        return false;
    const DateTime &now = snapshot.current;
    char time[12];
    format_configured_time(now, time, sizeof(time));
    if (g_time_format.show_seconds)
    {
        const size_t length = strlen(time);
        snprintf(time + length, sizeof(time) - length, ":%02u", now.second());
    }
    const uint8_t hour = configured_display_hour(now.hour());
    const char *value = source;
    size_t length = 0;
    while (*value && length + 1 < destination_size)
    {
        if (*value != '{')
        {
            destination[length++] = *value++;
            continue;
        }
        const char *end = strchr(value, '}');
        if (!end)
            return false;
        const size_t key_length = static_cast<size_t>(end - value - 1);
        const char *replacement = nullptr;
        char buffer[32] = {};
        if (key_length == 4 && !strncmp(value + 1, "time", 4))
            replacement = time;
        else if (key_length == 8 && !strncmp(value + 1, "time_min", 8))
        {
            format_configured_time(now, buffer, sizeof(buffer));
            replacement = buffer;
        }
        else if (key_length == 12 && !strncmp(value + 1, "time_seconds", 12))
        {
            snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", hour, now.minute(), now.second());
            replacement = buffer;
        }
        else if (key_length == 6 && !strncmp(value + 1, "minute", 6))
        {
            snprintf(buffer, sizeof(buffer), "%02u", now.minute()); replacement = buffer;
        }
        else if (key_length == 11 && !strncmp(value + 1, "minute_tens", 11))
        { snprintf(buffer, sizeof(buffer), "%02u", now.minute()); buffer[1] = '\0'; replacement = buffer; }
        else if (key_length == 11 && !strncmp(value + 1, "minute_ones", 11))
        { snprintf(buffer, sizeof(buffer), "%02u", now.minute()); buffer[0] = buffer[1]; buffer[1] = '\0'; replacement = buffer; }
        else if (key_length == 6 && !strncmp(value + 1, "second", 6))
        {
            snprintf(buffer, sizeof(buffer), "%02u", now.second()); replacement = buffer;
        }
        else if (key_length == 11 && !strncmp(value + 1, "second_tens", 11))
        { snprintf(buffer, sizeof(buffer), "%02u", now.second()); buffer[1] = '\0'; replacement = buffer; }
        else if (key_length == 11 && !strncmp(value + 1, "second_ones", 11))
        { snprintf(buffer, sizeof(buffer), "%02u", now.second()); buffer[0] = buffer[1]; buffer[1] = '\0'; replacement = buffer; }
        else if (key_length == 4 && !strncmp(value + 1, "hour", 4))
        {
            snprintf(buffer, sizeof(buffer), "%02u", hour); replacement = buffer;
        }
        else if (key_length == 9 && !strncmp(value + 1, "hour_tens", 9))
        { snprintf(buffer, sizeof(buffer), "%02u", hour); buffer[1] = '\0'; replacement = buffer; }
        else if (key_length == 9 && !strncmp(value + 1, "hour_ones", 9))
        { snprintf(buffer, sizeof(buffer), "%02u", hour); buffer[0] = buffer[1]; buffer[1] = '\0'; replacement = buffer; }
        else if (key_length == 8 && !strncmp(value + 1, "meridiem", 8))
            replacement = configured_meridiem(now.hour());
        else if (key_length == 4 && !strncmp(value + 1, "date", 4))
        {
            format_display_date(now, buffer, sizeof(buffer)); replacement = buffer;
        }
        else if (key_length == 7 && !strncmp(value + 1, "newline", 7))
            replacement = "\n";
        else if (key_length > 3 && !strncmp(value + 1, "tr.", 3))
        {
            char key[48] = {};
            if (key_length - 3 >= sizeof(key))
                return false;
            memcpy(key, value + 4, key_length - 3);
            JsonObjectConst translated = translations[key];
            const char *language = "en";
            switch (app_settings.language)
            {
            case UI_LANGUAGE_FRENCH: language = "fr"; break;
            case UI_LANGUAGE_SPANISH: language = "es"; break;
            case UI_LANGUAGE_GERMAN: language = "de"; break;
            case UI_LANGUAGE_ITALIAN: language = "it"; break;
            default: break;
            }
            replacement = translated[language] | translated["en"] | "";
        }
        else if (key_length == 4 && !strncmp(value + 1, "city", 4))
            replacement = snapshot.online.city;
        else if (key_length == 7 && !strncmp(value + 1, "weather", 7))
            replacement = snapshot.online.forecast_valid ? "Weather" : "--";
        else if (key_length == 13 && !strncmp(value + 1, "weather_asset", 13))
        {
            if (snapshot.online.forecast_valid)
            {
                replacement = snapshot.online.weather_code <= 1 ? "sunny" :
                    (snapshot.online.weather_code <= 3 ||
                     snapshot.online.weather_code == 45 ||
                     snapshot.online.weather_code == 48) ? "cloudy" : "rainy";
            }
            else
            {
                replacement = snapshot.sensor.condition == WeatherCondition::Sunny ? "sunny" :
                    snapshot.sensor.condition == WeatherCondition::Rainy ? "rainy" : "cloudy";
            }
        }
        else if (key_length == 13 && !strncmp(value + 1, "weekday_short", 13))
        {
            static const char *const weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
            replacement = weekdays[now.dayOfTheWeek() % 7];
        }
        else if (key_length == 13 && !strncmp(value + 1, "internal_temp", 13))
        {
            snprintf(buffer, sizeof(buffer), "%.1f", snapshot.sensor.valid ? snapshot.sensor.temperature : 0.0f); replacement = buffer;
        }
        else if (key_length == 13 && !strncmp(value + 1, "external_temp", 13))
        {
            snprintf(buffer, sizeof(buffer), "%.1f", snapshot.online.current_temperature); replacement = buffer;
        }
        else if (key_length == 16 && !strncmp(value + 1, "temperature_unit", 16))
            replacement = "C";
        else if (key_length == 12 && !strncmp(value + 1, "external_min", 12))
        {
            snprintf(buffer, sizeof(buffer), "%.0f", snapshot.online.minimum_temperature); replacement = buffer;
        }
        else if (key_length == 12 && !strncmp(value + 1, "external_max", 12))
        {
            snprintf(buffer, sizeof(buffer), "%.0f", snapshot.online.maximum_temperature); replacement = buffer;
        }
        else
            replacement = "";
        const size_t replacement_length = strlen(replacement);
        if (length + replacement_length >= destination_size)
            return false;
        memcpy(destination + length, replacement, replacement_length);
        length += replacement_length;
        value = end + 1;
    }
    destination[length] = '\0';
    return !*value;
}

static bool custom_face_visible(
    const char *expression, const ClockRenderSnapshot &snapshot)
{
    if (!expression || !expression[0])
        return true;
    const char *and_separator = strstr(expression, " and ");
    if (and_separator)
    {
        char left[32] = {};
        const size_t left_length = and_separator - expression;
        if (left_length >= sizeof(left)) return false;
        memcpy(left, expression, left_length);
        return custom_face_visible(left, snapshot) &&
            custom_face_visible(and_separator + 5, snapshot);
    }
    const bool negate = expression[0] == '!';
    const char *name = expression + (negate ? 1 : 0);
    bool value = false;
    if (!strcmp(name, "rtc_available")) value = true;
    else if (!strcmp(name, "weather_available"))
        value = snapshot.sensor.valid || snapshot.online.forecast_valid;
    else if (!strcmp(name, "wifi_available")) value = snapshot.online.connected;
    else if (!strcmp(name, "timer_active")) value = snapshot.timer_active;
    else if (!strcmp(name, "show_time_seconds"))
        value = g_time_format.show_seconds;
    else if (!strcmp(name, "floppy_inserted"))
        value = digitalRead(GPIO_FLOPPY) == LOW;
    else return false;
    return negate ? !value : value;
}

static void update_custom_line(
    CustomFaceDynamicLine &line, const ClockRenderSnapshot &snapshot)
{
    const DateTime &now = snapshot.current;
    const float precise_second = custom_precise_second(snapshot);
    float value = strtof(line.angle_template, nullptr);
    if (!strcmp(line.angle_template, "{hour}"))
        value = (now.hour() % 12) + now.minute() / 60.0f +
            precise_second / 3600.0f;
    else if (!strcmp(line.angle_template, "{minute}"))
        value = now.minute() + precise_second / 60.0f;
    else if (!strcmp(line.angle_template, "{second}"))
        value = precise_second;
    line.points[0] = {line.length, line.length};
    line.points[1] = {line.length, 0};
    lv_line_set_points(line.object, line.points, 2);
    lv_obj_set_size(line.object, line.length * 2, line.length * 2);
    lv_obj_set_pos(line.object, line.x - line.length, line.y - line.length);
    lv_obj_set_style_transform_pivot_x(line.object, line.length, 0);
    lv_obj_set_style_transform_pivot_y(line.object, line.length, 0);
    lv_obj_set_style_transform_rotation(line.object,
        static_cast<int32_t>(lroundf(value / line.maximum * 3600.0f)), 0);
}
}

bool ClockView::showCustomFace(const ClockRenderSnapshot &snapshot)
{
    const char *name = app_settings.custom_clock_face;
    if (!custom_face_name_valid(name))
        return false;
    if (!custom_face)
    {
        custom_face = create_clock_face_root(lv_screen_active(), lv_color_black());
        custom_loaded_name[0] = '\0';
    }
    if (strcmp(custom_loaded_name, name))
    {
        lv_obj_clean(custom_face);
        custom_loaded = false;
        custom_widget_count = 0;
        custom_line_count = 0;
        custom_loaded_name[0] = '\0';
        const String path = String("/clockface/") + name + "/clockface.json";
        File file = LittleFS.open(path.c_str(), "r");
        if (!file || file.size() > kCustomFaceMaxJsonBytes)
        {
            custom_face_error(*this, "PROJECT MISSING");
        }
        else
        {
            JsonDocument document;
            const DeserializationError error = deserializeJson(document, file);
            file.close();
            JsonArrayConst objects = document["objects"];
            JsonObjectConst translations = document["translations"];
            if (error || document["format"] != "maclock-clock-face" ||
                document["version"] != 1 || document["width"] != 304 ||
                document["height"] != 224 || objects.isNull() ||
                objects.size() > kCustomFaceMaxObjects)
            {
                custom_face_error(*this, "INVALID PROJECT");
            }
            else
            {
                lv_color_t background;
                if (!custom_face_color(document["background"] | "#000000", background))
                    background = lv_color_black();
                lv_obj_set_style_bg_color(custom_face, background, 0);
                bool valid = true;
                size_t object_index = 0;
                for (JsonObjectConst item : objects)
                {
                    if (!custom_face_visible(item["visible_if"] | "", snapshot))
                        continue;
                    const char *type = item["type"] | "";
                    const int16_t x = item["x"] | 0;
                    const int16_t y = item["y"] | 0;
                    const int16_t width = item["width"] | 0;
                    const int16_t height = item["height"] | 0;
                    if ((!strcmp(type, "line") &&
                         (abs(width) > 304 || abs(height) > 224)) ||
                        (strcmp(type, "line") &&
                         (width < 0 || height < 0 || width > 304 || height > 224)))
                    { valid = false; break; }
                    lv_color_t stroke;
                    if (!custom_face_color(item["stroke"] | "#000000", stroke))
                        stroke = lv_color_black();
                    lv_color_t fill;
                    const char *fill_text = item["fill"] | "transparent";
                    const bool filled = strcmp(fill_text, "transparent") != 0 && custom_face_color(fill_text, fill);
                    const int16_t stroke_width = item["stroke_width"] | 1;
                    const int16_t border_radius = item["border_radius"] |
                        (!strcmp(type, "circle") ? LV_RADIUS_CIRCLE :
                          !strcmp(type, "odometer_background") ||
                          !strcmp(type, "odometer") ? 6 : 0);
                    lv_obj_t *object = nullptr;
                    if (!strcmp(type, "rectangle") || !strcmp(type, "circle") || !strcmp(type, "odometer_background") || !strcmp(type, "flip_background"))
                    {
                        object = lv_obj_create(custom_face);
                        lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
                        lv_obj_set_pos(object, x, y);
                        lv_obj_set_size(object, width, height);
                        lv_obj_set_style_bg_opa(object, filled ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
                        if (filled) lv_obj_set_style_bg_color(object, fill, 0);
                        lv_obj_set_style_border_color(object, stroke, 0);
                        lv_obj_set_style_border_width(object, stroke_width, 0);
                        lv_obj_set_style_radius(object, border_radius, 0);
                    }
                    else if (!strcmp(type, "line"))
                    {
                        object = lv_line_create(custom_face);
                        lv_obj_remove_style_all(object);
                        custom_line_points[object_index][0] = {0, 0};
                        char angle[48];
                        if (!custom_face_expand(
                                item["angle"] | "", snapshot, translations,
                                angle, sizeof(angle)))
                        { valid = false; break; }
                        float value = strtof(angle, nullptr);
                        const float maximum = item["max"].as<float>();
                        if (angle[0] && maximum > 0.0f)
                        {
                            const char *angle_template = item["angle"] | "";
                            const DateTime &now = snapshot.current;
                            const float precise_second = custom_precise_second(snapshot);
                            if (!strcmp(angle_template, "{hour}"))
                                value = (now.hour() % 12) +
                                    now.minute() / 60.0f +
                                    precise_second / 3600.0f;
                            else if (!strcmp(angle_template, "{minute}"))
                                value = now.minute() + precise_second / 60.0f;
                            else if (!strcmp(angle_template, "{second}"))
                                value = precise_second;
                            const float radians = value / maximum * 6.28318530718f;
                            const float length = abs(height ? height : width);
                            custom_line_points[object_index][1] = {
                                (lv_value_precise_t)lroundf(sinf(radians) * length),
                                (lv_value_precise_t)lroundf(-cosf(radians) * length)};
                        }
                        else
                            custom_line_points[object_index][1] = {width, height};
                        const lv_value_precise_t end_x =
                            custom_line_points[object_index][1].x;
                        const lv_value_precise_t end_y =
                            custom_line_points[object_index][1].y;
                        const lv_value_precise_t min_x =
                            end_x < 0 ? end_x : 0;
                        const lv_value_precise_t min_y =
                            end_y < 0 ? end_y : 0;
                        custom_line_points[object_index][0] = {-min_x, -min_y};
                        custom_line_points[object_index][1] = {
                            end_x - min_x, end_y - min_y};
                        lv_line_set_points(
                            object, custom_line_points[object_index], 2);
                        lv_obj_set_pos(object, x + min_x, y + min_y);
                        lv_obj_set_style_line_color(object, stroke, 0);
                        lv_obj_set_style_line_width(object, stroke_width, 0);
                        const char *angle_template = item["angle"] | "";
                        if (angle_template[0] && maximum > 0.0f &&
                            custom_line_count <
                                sizeof(custom_lines) / sizeof(custom_lines[0]))
                        {
                            CustomFaceDynamicLine &line =
                                custom_lines[custom_line_count++];
                            line = {};
                            line.object = object;
                            line.x = x;
                            line.y = y;
                            line.length = abs(height ? height : width);
                            line.maximum = maximum;
                            strlcpy(line.angle_template, angle_template,
                                sizeof(line.angle_template));
                            update_custom_line(line, snapshot);
                        }
                    }
                    else if (!strcmp(type, "text"))
                    {
                        const lv_font_t *font = custom_face_font(item["font_family"] | "lv_font_chicago_8");
                        if (!font) { valid = false; break; }
                        object = create_clock_face_label(custom_face, font, stroke);
                        lv_obj_set_pos(object, x, y);
                        lv_obj_set_width(object, width);
                        lv_label_set_long_mode(object, LV_LABEL_LONG_CLIP);
                        const char *align = item["align"] | "left";
                        lv_obj_set_style_text_align(object, !strcmp(align, "right") ? LV_TEXT_ALIGN_RIGHT : !strcmp(align, "center") ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT, 0);
                        char text[192];
                        const JsonArrayConst random = item["random"];
                        const char *template_text = item["template"] | "";
                        if (!random.isNull() && random.size())
                        {
                            const uint32_t interval = document["random_interval_seconds"] | 60;
                            template_text = random[(snapshot.current.unixtime() / (interval ? interval : 1)) % random.size()] | template_text;
                        }
                        if (!custom_face_expand(template_text, snapshot, translations, text, sizeof(text)))
                        { valid = false; break; }
                        const char *face_name = document["name"] | "";
                        if (g_face_customization.colon_blink != ColonBlinkInterval::None &&
                            strcmp(face_name, "Departure Board") &&
                            (strstr(template_text, "{hour}") ||
                             strstr(template_text, "{time}") ||
                             strstr(template_text, "{time_min}") ||
                             strstr(template_text, "{time_seconds}")) &&
                            strchr(text, ':'))
                        {
                            lv_obj_delete(object);
                            object = nullptr;
                            const int16_t total_width = custom_text_width(font, text);
                            int16_t cursor_x = !strcmp(align, "right")
                                ? width - total_width : !strcmp(align, "center")
                                ? (width - total_width) / 2 : 0;
                            const char *segment_start = text;
                            for (const char *cursor = text;; ++cursor)
                            {
                                if (*cursor != ':' && *cursor != '\0') continue;
                                char segment[64] = {};
                                const size_t segment_length = min(
                                    static_cast<size_t>(cursor - segment_start),
                                    sizeof(segment) - 1);
                                memcpy(segment, segment_start, segment_length);
                                const int16_t segment_width =
                                    custom_text_width(font, segment);
                                if (segment_length)
                                {
                                    lv_obj_t *part = create_clock_face_label(
                                        custom_face, font, stroke);
                                    lv_label_set_text(part, segment);
                                    lv_obj_set_pos(part, x + cursor_x, y);
                                    lv_obj_set_width(part, segment_width);
                                    lv_obj_set_style_text_align(part,
                                        LV_TEXT_ALIGN_LEFT, 0);
                                    if (!object) object = part;
                                    cursor_x += segment_width;
                                }
                                if (*cursor == ':')
                                {
                                    const int16_t colon_width =
                                        custom_text_width(font, ":");
                                    if (custom_colon_visible(snapshot))
                                    {
                                        lv_obj_t *colon = create_clock_face_label(
                                            custom_face, font, stroke);
                                        lv_label_set_text(colon, ":");
                                        lv_obj_set_pos(colon, x + cursor_x, y);
                                        lv_obj_set_width(colon, colon_width);
                                        lv_obj_set_style_text_align(colon,
                                            LV_TEXT_ALIGN_LEFT, 0);
                                        if (!object) object = colon;
                                    }
                                    cursor_x += colon_width;
                                    segment_start = cursor + 1;
                                }
                                else break;
                            }
                            if (!object)
                                object = lv_obj_create(custom_face);
                        }
                        else
                            lv_label_set_text(object, text);
                    }
                    else if (!strcmp(type, "image"))
                    {
                        char asset[128];
                        if (!custom_face_expand(item["template"] | item["source"] | "", snapshot, translations, asset, sizeof(asset)) || !asset[0] || strchr(asset, '/'))
                        { valid = false; break; }
                        const String asset_path = String("/clockface/") + name + "/" + asset;
                        if (!LittleFS.exists(asset_path.c_str())) { valid = false; break; }
                        object = lv_image_create(custom_face);
                        lv_obj_set_pos(object, x, y);
                        lv_obj_set_size(object, width, height);
                        snprintf(
                            custom_image_paths[object_index],
                            sizeof(custom_image_paths[object_index]),
                            "S:%s", asset_path.c_str());
                        lv_image_set_src(
                            object, custom_image_paths[object_index]);
                    }
                    else if (!strcmp(type, "flip") || !strcmp(type, "odometer"))
                    {
                        if (custom_widget_count >=
                            sizeof(custom_widgets) / sizeof(custom_widgets[0]))
                        { valid = false; break; }
                        const lv_font_t *font = custom_face_font(
                            item["font_family"] | "lv_font_chicago_48");
                        if (!font) { valid = false; break; }
                        CustomFaceWidget &widget =
                            custom_widgets[custom_widget_count++];
                        widget = {};
                        widget.type = !strcmp(type, "flip")
                            ? CustomFaceWidgetType::Flip
                            : CustomFaceWidgetType::Odometer;
                        strlcpy(widget.template_text,
                            item["template"] | "",
                            sizeof(widget.template_text));
                        widget.width = width;
                        widget.height = height;
                        widget.root = lv_obj_create(custom_face);
                        lv_obj_remove_style_all(widget.root);
                        lv_obj_remove_flag(widget.root, LV_OBJ_FLAG_SCROLLABLE);
                        lv_obj_set_pos(widget.root, x, y);
                        lv_obj_set_size(widget.root, width, height);
                        lv_obj_set_style_bg_color(widget.root,
                            filled ? fill : widget.type == CustomFaceWidgetType::Flip
                                ? lv_color_hex(0x181818) : lv_color_hex(0x080808), 0);
                        lv_obj_set_style_bg_opa(widget.root,
                            widget.type == CustomFaceWidgetType::Flip ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
                        lv_obj_set_style_border_color(widget.root, stroke, 0);
                        lv_obj_set_style_border_width(widget.root,
                            widget.type == CustomFaceWidgetType::Odometer ||
                            widget.type == CustomFaceWidgetType::Flip
                                ? 0 : stroke_width, 0);
                        lv_obj_set_style_radius(widget.root, border_radius, 0);
                        widget.font = font;
                        widget.stroke = stroke;
                        const char *alignment = item["align"] | "center";
                        widget.align = !strcmp(alignment, "left")
                            ? LV_TEXT_ALIGN_LEFT : !strcmp(alignment, "right")
                            ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_CENTER;
                        char text[64];
                        if (!custom_face_expand(widget.template_text, snapshot, translations, text, sizeof(text)))
                        { valid = false; break; }
                        custom_widget_set_text(widget, text, true);
                    }
                    else if (!strcmp(type, "colon"))
                    {
                        if (custom_widget_count >= sizeof(custom_widgets) / sizeof(custom_widgets[0]))
                        { valid = false; break; }
                        CustomFaceWidget &widget = custom_widgets[custom_widget_count++];
                        widget = {};
                        widget.type = CustomFaceWidgetType::Colon;
                        widget.blink = item["blink"].is<bool>()
                            ? item["blink"].as<bool>() : true;
                        widget.root = lv_obj_create(custom_face);
                        lv_obj_remove_style_all(widget.root);
                        lv_obj_remove_flag(widget.root, LV_OBJ_FLAG_SCROLLABLE);
                        lv_obj_set_pos(widget.root, x, y);
                        lv_obj_set_size(widget.root, width, height);
                        for (const int16_t dot_y : {height / 3 - 2, height * 2 / 3 - 2})
                        {
                            lv_obj_t *dot = lv_obj_create(widget.root);
                            lv_obj_remove_style_all(dot);
                            lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_size(dot, 4, 4);
                            lv_obj_set_pos(dot, (width - 4) / 2, dot_y);
                            lv_obj_set_style_bg_color(dot, stroke, 0);
                            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
                            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
                        }
                    }
                    else
                    { valid = false; break; }
                    ++object_index;
                }
                if (!valid)
                    custom_face_error(*this, "UNSUPPORTED LAYER");
                else
                {
                    strlcpy(custom_loaded_name, name, sizeof(custom_loaded_name));
                    custom_loaded = true;
                }
            }
        }
    }
    lv_obj_clear_flag(custom_face, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(custom_face);
    return true;
}

void ClockView::updateCustomFace(const ClockRenderSnapshot &snapshot)
{
    if (custom_widget_count)
    {
        refreshCustomFaceWidgets(snapshot);
        return;
    }
    if (custom_line_count)
    {
        refreshCustomFaceLines(snapshot);
        return;
    }
    if (custom_loaded)
    {
        const uint32_t now = millis();
        if (custom_last_refresh_ms &&
            now - custom_last_refresh_ms < custom_face_refresh_interval())
            return;
        custom_last_refresh_ms = now;
        custom_loaded_name[0] = '\0';
        showCustomFace(snapshot);
    }
}

void ClockView::refreshCustomFaceLines(
    const ClockRenderSnapshot &snapshot)
{
    static uint32_t last_update = 0;
    const uint32_t now = millis();
    if (last_update && now - last_update < custom_second_refresh_interval())
        return;
    last_update = now;
    for (size_t i = 0; i < custom_line_count; ++i)
        update_custom_line(custom_lines[i], snapshot);
}

void ClockView::refreshCustomFaceWidgets(
    const ClockRenderSnapshot &snapshot)
{
    for (size_t i = 0; i < custom_widget_count; ++i)
    {
        CustomFaceWidget &widget = custom_widgets[i];
        if (widget.type == CustomFaceWidgetType::Colon)
        {
            lv_obj_set_style_opa(widget.root,
                !widget.blink || custom_colon_visible(snapshot) ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            continue;
        }
        char text[64];
        if (custom_face_expand(widget.template_text, snapshot, {}, text, sizeof(text)))
            custom_widget_set_text(widget, text, false);
    }
}

void ClockView::hideCustomFace()
{
    if (custom_face)
        lv_obj_add_flag(custom_face, LV_OBJ_FLAG_HIDDEN);
}
#endif
