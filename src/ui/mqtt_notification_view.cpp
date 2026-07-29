#ifdef MACLOCK_COMBINED_SOURCE
namespace
{
void mqtt_notification_ok_event(lv_event_t *)
{
    mqtt_service.acknowledgeCurrent();
}
}

void MqttNotificationView::init(lv_obj_t *screen)
{
    panel = lv_obj_create(screen);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(panel, 272, 164);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);

    title = lv_label_create(panel);
    lv_obj_set_width(title, 240);
    lv_obj_set_style_text_font(title, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    message = lv_label_create(panel);
    lv_obj_set_size(message, 240, 100);
    lv_label_set_long_mode(message, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(message, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_line_space(message, 3, 0);
    lv_obj_align(message, LV_ALIGN_TOP_LEFT, 6, 24);

    ok_button = create_action_button(
        panel, "OK", mqtt_notification_ok_event);
    lv_obj_set_size(ok_button, 82, 28);
    lv_obj_align(ok_button, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

void MqttNotificationView::show(const MqttMessage &incoming)
{
    if (!panel)
        return;
    lv_label_set_text(title, incoming.title);
    lv_label_set_text(message, incoming.message);
    if (incoming.kind == MqttMessageKind::Notification)
        lv_obj_clear_flag(ok_button, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(ok_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(panel);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
    lv_timer_handler();
}

void MqttNotificationView::hide()
{
    if (!panel)
        return;
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    lv_timer_handler();
}
#endif
