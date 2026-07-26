#include "selector_list_style.h"

LV_FONT_DECLARE(lv_font_chicago_8);

void selector_list_style_container(lv_obj_t *list)
{
    if (!list)
        return;

    lv_obj_set_style_bg_color(list, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, 4, 0);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
}

void selector_list_style_item(lv_obj_t *item)
{
    if (!item)
        return;

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
