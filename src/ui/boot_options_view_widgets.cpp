#ifdef MACLOCK_COMBINED_SOURCE
static void boot_options_continue_visual_event(lv_event_t *event)
{
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(event);
    const lv_event_code_t code = lv_event_get_code(event);
    lv_obj_set_style_text_color(
        label,
        code == LV_EVENT_PRESSED ? lv_color_white() : lv_color_black(),
        0);
}

static void hard_pixel_button_draw_event(lv_event_t *event)
{
    lv_draw_task_t *task = lv_event_get_draw_task(event);
    if (!task)
        return;

    lv_draw_dsc_base_t *base =
        static_cast<lv_draw_dsc_base_t *>(
            lv_draw_task_get_draw_dsc(task));
    if (!base ||
        (base->part != LV_PART_MAIN &&
         base->part != LV_PART_ITEMS))
    {
        return;
    }

    if (lv_draw_fill_dsc_t *fill =
            lv_draw_task_get_fill_dsc(task))
    {
        fill->radius = 0;
    }

    lv_draw_border_dsc_t *border =
        lv_draw_task_get_border_dsc(task);
    if (!border)
        return;
    border->radius = 0;

    lv_area_t area;
    lv_draw_task_get_area(task, &area);
    const lv_point_t corners[] = {
        {area.x1, area.y1},
        {area.x2, area.y1},
        {area.x1, area.y2},
        {area.x2, area.y2},
    };
    lv_draw_fill_dsc_t corner;
    lv_draw_fill_dsc_init(&corner);
    corner.color = lv_color_white();
    corner.opa = LV_OPA_COVER;
    for (const lv_point_t &point : corners)
    {
        lv_area_t pixel = {
            point.x, point.y, point.x, point.y};
        lv_draw_fill(base->layer, &corner, &pixel);
    }
}

static void enable_hard_pixel_button_drawing(lv_obj_t *object)
{
    lv_obj_add_flag(
        object, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(
        object, hard_pixel_button_draw_event,
        LV_EVENT_DRAW_TASK_ADDED, nullptr);
}

static void style_boot_options_matrix(lv_obj_t *matrix)
{
    const lv_style_selector_t checked_items =
        (lv_style_selector_t)LV_PART_ITEMS |
        (lv_style_selector_t)LV_STATE_CHECKED;
    const lv_style_selector_t pressed_items =
        (lv_style_selector_t)LV_PART_ITEMS |
        (lv_style_selector_t)LV_STATE_PRESSED;

    lv_obj_set_style_bg_opa(matrix, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(matrix, 0, 0);
    lv_obj_set_style_radius(matrix, 0, 0);
    lv_obj_set_style_pad_all(matrix, 0, 0);
    lv_obj_set_style_pad_row(matrix, 10, 0);
    lv_obj_set_style_pad_column(matrix, 10, 0);
    lv_obj_set_style_text_font(matrix, &lv_font_chicago_8, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_border_color(matrix, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(matrix, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(matrix, 1, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(matrix, 0, LV_PART_ITEMS);
    lv_obj_set_style_outline_width(matrix, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, lv_color_black(), checked_items);
    lv_obj_set_style_text_color(matrix, lv_color_white(), checked_items);
    lv_obj_set_style_bg_color(matrix, lv_color_black(), pressed_items);
    lv_obj_set_style_text_color(matrix, lv_color_white(), pressed_items);
    enable_hard_pixel_button_drawing(matrix);
}

static lv_obj_t *create_boot_checkbox(
    lv_obj_t *parent, const char *text,
    lv_event_cb_t callback)
{
    lv_obj_t *checkbox = lv_checkbox_create(parent);
    lv_checkbox_set_text(checkbox, text);
    lv_obj_set_style_text_font(
        checkbox, &lv_font_chicago_8, LV_PART_MAIN);
    lv_obj_set_style_text_color(
        checkbox, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        checkbox, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(
        checkbox, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        checkbox, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(
        checkbox, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(
        checkbox, 1, LV_PART_MAIN);
    lv_obj_set_style_outline_width(
        checkbox, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(
        checkbox, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_top(
        checkbox, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_left(
        checkbox, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_column(
        checkbox, 8, LV_PART_MAIN);

    lv_obj_set_style_pad_all(
        checkbox, 4, LV_PART_INDICATOR);
    lv_obj_set_style_radius(
        checkbox, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(
        checkbox, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(
        checkbox, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_border_color(
        checkbox, lv_color_black(), LV_PART_INDICATOR);
    lv_obj_set_style_border_width(
        checkbox, 1, LV_PART_INDICATOR);
    lv_obj_set_style_shadow_width(
        checkbox, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_image_src(
        checkbox, nullptr, LV_PART_INDICATOR);

    const lv_style_selector_t checked_indicator =
        (lv_style_selector_t)LV_PART_INDICATOR |
        (lv_style_selector_t)LV_STATE_CHECKED;
    lv_obj_set_style_bg_color(
        checkbox, lv_color_white(), checked_indicator);
    lv_obj_set_style_bg_image_src(
        checkbox, LV_SYMBOL_OK, checked_indicator);
    lv_obj_set_style_bg_image_recolor(
        checkbox, lv_color_black(), checked_indicator);
    lv_obj_set_style_bg_image_recolor_opa(
        checkbox, LV_OPA_COVER, checked_indicator);
    lv_obj_set_style_text_font(
        checkbox, LV_FONT_DEFAULT, checked_indicator);
    lv_obj_set_style_text_color(
        checkbox, lv_color_black(), checked_indicator);

    const lv_style_selector_t pressed_main =
        (lv_style_selector_t)LV_PART_MAIN |
        (lv_style_selector_t)LV_STATE_PRESSED;
    lv_obj_set_style_bg_color(
        checkbox, lv_color_black(), pressed_main);
    lv_obj_set_style_text_color(
        checkbox, lv_color_white(), pressed_main);

    lv_obj_set_ext_click_area(checkbox, 0);
    enable_hard_pixel_button_drawing(checkbox);
    lv_obj_add_event_cb(
        checkbox, callback, LV_EVENT_VALUE_CHANGED, nullptr);
    return checkbox;
}

static lv_obj_t *create_action_button(lv_obj_t *parent,
                                      const char *text,
                                      lv_event_cb_t callback)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_style_bg_color(button, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(button, lv_color_black(), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_radius(button, 1, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_outline_width(button, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_black(), LV_STATE_PRESSED);
    enable_hard_pixel_button_drawing(button);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(button, boot_options_continue_visual_event,
                        LV_EVENT_PRESSED, label);
    lv_obj_add_event_cb(button, boot_options_continue_visual_event,
                        LV_EVENT_RELEASED, label);
    lv_obj_add_event_cb(button, boot_options_continue_visual_event,
                        LV_EVENT_PRESS_LOST, label);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
    return button;
}

static lv_obj_t *create_boot_options_page(lv_obj_t *parent)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, 276, 130);
    lv_obj_align(page, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    return page;
}

#endif
