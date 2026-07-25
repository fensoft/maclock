#pragma once

#include <stdint.h>

static constexpr uint8_t kBrightnessMax = 12;

static inline uint8_t brightness_to_pwm(int level)
{
    static constexpr uint8_t curve[kBrightnessMax + 1] = {
        0, 8, 13, 21, 32, 46, 63, 84, 109, 139, 173, 212, 255};

    if (level < 0)
        level = 0;
    if (level > kBrightnessMax)
        level = kBrightnessMax;
    return curve[level];
}
