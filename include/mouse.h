#pragma once

#include <stdint.h>

class MouseClass
{
public:
    void Init();
    void Read(float &x, float &y);
    void Update();
    bool ReadButton();

private:
    void ResetMotion();
    void EmitAccumulatedMotion(float &x, float &y);

    bool has_last_ = false;
    uint16_t last_raw_x_ = 0;
    uint16_t last_raw_y_ = 0;
    float filtered_x_ = 0.0f;
    float filtered_y_ = 0.0f;
    float accumulated_x_ = 0.0f;
    float accumulated_y_ = 0.0f;
    float last_gain_ = 0.5f;
};

extern MouseClass Mouse;
