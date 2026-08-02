#include "rotary_navigator.h"

namespace
{
bool is_button_matrix(lv_obj_t *object)
{
    return lv_obj_has_class(object, &lv_buttonmatrix_class);
}
}

bool RotaryNavigator::eligible(lv_obj_t *object) const
{
    if (!object || lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN) ||
        lv_obj_has_state(object, LV_STATE_DISABLED))
        return false;
    return is_button_matrix(object) ||
           lv_obj_has_class(object, &lv_button_class) ||
           lv_obj_has_class(object, &lv_list_button_class) ||
           lv_obj_has_class(object, &lv_dropdown_class) ||
           lv_obj_has_class(object, &lv_checkbox_class) ||
           lv_obj_has_class(object, &lv_textarea_class) ||
           lv_obj_has_class(object, &lv_switch_class) ||
           lv_obj_has_class(object, &lv_roller_class) ||
           lv_obj_has_class(object, &lv_slider_class);
}

void RotaryNavigator::collect(lv_obj_t *object, bool ancestors_visible)
{
    const bool visible = ancestors_visible && object &&
                         !lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN);
    if (!visible)
        return;

    if (eligible(object))
    {
        if (is_button_matrix(object))
        {
            const char *const *map = lv_buttonmatrix_get_map(object);
            uint32_t button = 0;
            for (size_t i = 0; map && map[i] && map[i][0]; ++i)
            {
                if (map[i][0] == '\n' && map[i][1] == '\0')
                    continue;
                if (lv_buttonmatrix_has_button_ctrl(
                        object, button,
                        static_cast<lv_buttonmatrix_ctrl_t>(
                            LV_BUTTONMATRIX_CTRL_DISABLED |
                            LV_BUTTONMATRIX_CTRL_HIDDEN)))
                {
                    ++button;
                    continue;
                }
                if (count_ < kMaxTargets)
                    targets_[count_++] = {object, button};
                ++button;
            }
        }
        else if (count_ < kMaxTargets)
        {
            targets_[count_++] = {object, UINT32_MAX};
        }
    }

    const uint32_t children = lv_obj_get_child_count(object);
    for (uint32_t i = 0; i < children; ++i)
        collect(lv_obj_get_child(object, i), visible);
}

void RotaryNavigator::refresh()
{
    lv_obj_t *previous_object = focused_object_;
    const uint32_t previous_button = focused_button_;
    count_ = 0;
    collect(lv_screen_active(), true);
    selected_ = 0;
    for (size_t i = 0; i < count_; ++i)
    {
        if (targets_[i].object == previous_object &&
            targets_[i].button == previous_button)
        {
            selected_ = i;
            break;
        }
    }
    applyFocus();
}

void RotaryNavigator::applyFocus()
{
    if (focused_object_)
    {
        lv_obj_clear_state(focused_object_, LV_STATE_FOCUSED);
    }
    focused_object_ = nullptr;
    focused_button_ = UINT32_MAX;
    if (!count_)
        return;
    const Target &target = targets_[selected_];
    focused_object_ = target.object;
    focused_button_ = target.button;
    blink_visible_ = true;
    if (target.button != UINT32_MAX)
    {
        lv_buttonmatrix_set_selected_button(target.object, target.button);
        lv_obj_set_style_bg_color(
            target.object, lv_color_black(),
            LV_PART_ITEMS | LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(
            target.object, lv_color_white(),
            LV_PART_ITEMS | LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(
            target.object, 2,
            LV_PART_ITEMS | LV_STATE_FOCUSED);
    }
    lv_obj_add_state(target.object, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(
        target.object, lv_color_black(), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(
        target.object, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(
        target.object, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_scroll_to_view(target.object, LV_ANIM_OFF);
    applyBlinkStyle();
    if (blink_timer_)
        lv_timer_reset(blink_timer_);
}

void RotaryNavigator::applyBlinkStyle()
{
    if (!focused_object_)
        return;

    if (focused_button_ != UINT32_MAX)
    {
        lv_obj_set_style_bg_opa(
            focused_object_, blink_visible_ ? LV_OPA_COVER : LV_OPA_20,
            LV_PART_ITEMS | LV_STATE_FOCUSED);
        lv_obj_set_style_text_opa(
            focused_object_, blink_visible_ ? LV_OPA_COVER : LV_OPA_50,
            LV_PART_ITEMS | LV_STATE_FOCUSED);
        lv_obj_set_style_border_opa(
            focused_object_, blink_visible_ ? LV_OPA_COVER : LV_OPA_30,
            LV_PART_ITEMS | LV_STATE_FOCUSED);
    }
    else
    {
        lv_obj_set_style_outline_opa(
            focused_object_, blink_visible_ ? LV_OPA_COVER : LV_OPA_20,
            LV_PART_MAIN | LV_STATE_FOCUSED);
    }
    lv_obj_invalidate(focused_object_);
}

void RotaryNavigator::blinkTimerThunk(lv_timer_t *timer)
{
    auto *navigator = static_cast<RotaryNavigator *>(lv_timer_get_user_data(timer));
    if (!navigator || !navigator->focused_object_)
        return;
    navigator->blink_visible_ = !navigator->blink_visible_;
    navigator->applyBlinkStyle();
}

void RotaryNavigator::enter()
{
    focused_object_ = nullptr;
    focused_button_ = UINT32_MAX;
    if (!blink_timer_)
        blink_timer_ = lv_timer_create(blinkTimerThunk, 450, this);
    refresh();
}

void RotaryNavigator::leave()
{
    if (focused_object_)
        lv_obj_clear_state(focused_object_, LV_STATE_FOCUSED);
    focused_object_ = nullptr;
    focused_button_ = UINT32_MAX;
    count_ = 0;
    blink_visible_ = true;
    if (blink_timer_)
    {
        lv_timer_delete(blink_timer_);
        blink_timer_ = nullptr;
    }
}

void RotaryNavigator::move(int direction)
{
    refresh();
    if (!count_ || !direction)
        return;
    if (focused_object_ &&
        lv_obj_has_class(focused_object_, &lv_dropdown_class) &&
        lv_dropdown_is_open(focused_object_))
    {
        const uint32_t count =
            lv_dropdown_get_option_count(focused_object_);
        if (count)
        {
            int selected = static_cast<int>(
                lv_dropdown_get_selected(focused_object_));
            selected += direction > 0 ? 1 : -1;
            if (selected < 0)
                selected = static_cast<int>(count - 1);
            if (selected >= static_cast<int>(count))
                selected = 0;
            lv_dropdown_set_selected(
                focused_object_,
                static_cast<uint32_t>(selected));
            lv_obj_send_event(
                focused_object_, LV_EVENT_VALUE_CHANGED, nullptr);
        }
        return;
    }
    if (direction > 0)
        selected_ = (selected_ + 1) % count_;
    else
        selected_ = (selected_ + count_ - 1) % count_;
    applyFocus();
}

void RotaryNavigator::activate()
{
    refresh();
    if (!count_)
        return;
    const Target target = targets_[selected_];
    if (target.button != UINT32_MAX)
    {
        lv_buttonmatrix_set_selected_button(target.object, target.button);
        if (lv_buttonmatrix_has_button_ctrl(
                target.object, target.button,
                LV_BUTTONMATRIX_CTRL_CHECKABLE))
        {
            const bool checked =
                lv_buttonmatrix_has_button_ctrl(
                    target.object, target.button,
                    LV_BUTTONMATRIX_CTRL_CHECKED);
            if (lv_buttonmatrix_get_one_checked(target.object))
                lv_buttonmatrix_clear_button_ctrl_all(
                    target.object,
                    LV_BUTTONMATRIX_CTRL_CHECKED);
            if (!checked ||
                lv_buttonmatrix_get_one_checked(target.object))
            {
                lv_buttonmatrix_set_button_ctrl(
                    target.object, target.button,
                    LV_BUTTONMATRIX_CTRL_CHECKED);
            }
            else
            {
                lv_buttonmatrix_clear_button_ctrl(
                    target.object, target.button,
                    LV_BUTTONMATRIX_CTRL_CHECKED);
            }
        }
        lv_obj_send_event(target.object, LV_EVENT_VALUE_CHANGED, nullptr);
    }
    else
    {
        lv_obj_send_event(target.object, LV_EVENT_CLICKED, nullptr);
    }
    refresh();
}
