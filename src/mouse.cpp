#include "mouse.h"

#include "TouchSensor.h"
#include "touch.h"

extern TouchSensor touch;

MouseClass Mouse;

namespace
{
static constexpr int kMouseOutlierThreshold = 120;
static constexpr int kMouseFineSpeed = 3;
static constexpr int kMouseNormalSpeed = 8;
static constexpr int kMouseFastSpeed = 16;
static constexpr float kMouseFineGain = 0.5f;
static constexpr float kMouseNormalGain = 1.0f;
static constexpr float kMouseFastGain = 1.5f;
static constexpr float kMouseMinimumFilterAlpha = 0.35f;
static constexpr int kMouseFullResponseSpeed = 6;

static int AbsoluteValue(int value)
{
    return value < 0 ? -value : value;
}

static float MouseFilterAlpha(int speed)
{
    if (speed <= 2)
        return kMouseMinimumFilterAlpha;
    if (speed >= kMouseFullResponseSpeed)
        return 1.0f;

    return kMouseMinimumFilterAlpha +
           (float)(speed - 2) *
               ((1.0f - kMouseMinimumFilterAlpha) /
                (float)(kMouseFullResponseSpeed - 2));
}

static float MouseGain(int speed)
{
    if (speed <= kMouseFineSpeed)
        return kMouseFineGain;
    if (speed <= kMouseNormalSpeed)
    {
        return kMouseFineGain +
               (float)(speed - kMouseFineSpeed) *
                   ((kMouseNormalGain - kMouseFineGain) /
                    (float)(kMouseNormalSpeed - kMouseFineSpeed));
    }
    if (speed >= kMouseFastSpeed)
        return kMouseFastGain;

    return kMouseNormalGain +
           (float)(speed - kMouseNormalSpeed) *
               ((kMouseFastGain - kMouseNormalGain) /
                (float)(kMouseFastSpeed - kMouseNormalSpeed));
}
}

void MouseClass::Init()
{
    ResetMotion();
}

void MouseClass::ResetMotion()
{
    has_last_ = false;
    last_raw_x_ = 0;
    last_raw_y_ = 0;
    filtered_x_ = 0.0f;
    filtered_y_ = 0.0f;
    accumulated_x_ = 0.0f;
    accumulated_y_ = 0.0f;
    last_gain_ = kMouseFineGain;
}

void MouseClass::EmitAccumulatedMotion(float &x, float &y)
{
    const int output_x = (int)accumulated_x_;
    const int output_y = (int)accumulated_y_;
    accumulated_x_ -= (float)output_x;
    accumulated_y_ -= (float)output_y;
    x = (float)output_x;
    y = (float)output_y;
}

void MouseClass::Read(float &x, float &y)
{
    x = 0.0f;
    y = 0.0f;

    uint16_t raw_x = 0;
    uint16_t raw_y = 0;
    if (!touch_read_raw(raw_x, raw_y))
    {
        if (has_last_)
        {
            accumulated_x_ +=
                ((float)last_raw_x_ - filtered_x_) * last_gain_;
            accumulated_y_ +=
                ((float)last_raw_y_ - filtered_y_) * last_gain_;
            EmitAccumulatedMotion(x, y);
        }
        ResetMotion();
        return;
    }

    if (!has_last_)
    {
        has_last_ = true;
        last_raw_x_ = raw_x;
        last_raw_y_ = raw_y;
        filtered_x_ = (float)raw_x;
        filtered_y_ = (float)raw_y;
        return;
    }

    const int raw_dx = (int)raw_x - (int)last_raw_x_;
    const int raw_dy = (int)raw_y - (int)last_raw_y_;
    last_raw_x_ = raw_x;
    last_raw_y_ = raw_y;

    const int absolute_dx = AbsoluteValue(raw_dx);
    const int absolute_dy = AbsoluteValue(raw_dy);
    if (absolute_dx > kMouseOutlierThreshold ||
        absolute_dy > kMouseOutlierThreshold)
    {
        filtered_x_ = (float)raw_x;
        filtered_y_ = (float)raw_y;
        accumulated_x_ = 0.0f;
        accumulated_y_ = 0.0f;
        return;
    }

    const int speed =
        absolute_dx > absolute_dy ? absolute_dx : absolute_dy;
    const float filter_alpha = MouseFilterAlpha(speed);
    const float previous_filtered_x = filtered_x_;
    const float previous_filtered_y = filtered_y_;
    filtered_x_ += filter_alpha * ((float)raw_x - filtered_x_);
    filtered_y_ += filter_alpha * ((float)raw_y - filtered_y_);

    const float gain = MouseGain(speed);
    last_gain_ = gain;
    accumulated_x_ += (filtered_x_ - previous_filtered_x) * gain;
    accumulated_y_ += (filtered_y_ - previous_filtered_y) * gain;
    EmitAccumulatedMotion(x, y);
}

void MouseClass::Update()
{
}

bool MouseClass::ReadButton()
{
    return touch.update();
}
