#pragma once

#include <stddef.h>
#include <stdint.h>
#include <lvgl.h>

class RotaryNavigator
{
public:
    void enter();
    void leave();
    void move(int direction);
    void activate();
    void refresh();

private:
    struct Target
    {
        lv_obj_t *object = nullptr;
        uint32_t button = UINT32_MAX;
    };

    static constexpr size_t kMaxTargets = 96;
    Target targets_[kMaxTargets] = {};
    size_t count_ = 0;
    size_t selected_ = 0;
    lv_obj_t *focused_object_ = nullptr;
    uint32_t focused_button_ = UINT32_MAX;
    lv_timer_t *blink_timer_ = nullptr;
    bool blink_visible_ = true;

    void collect(lv_obj_t *object, bool ancestors_visible);
    void applyFocus();
    void applyBlinkStyle();
    bool eligible(lv_obj_t *object) const;
    static void blinkTimerThunk(lv_timer_t *timer);
};
