#include "mouse.h"

#include "TouchSensor.h"
#include "touch.h"
#include "touch.h"
extern TouchSensor touch;

MouseClass Mouse;

void MouseClass::Init()
{
}

void MouseClass::Read(float &x, float &y)
{
    static bool has_last = false;
    static uint16_t last_x = 0;
    static uint16_t last_y = 0;

    uint16_t raw_x = 0;
    uint16_t raw_y = 0;
    if (touch_read_raw(raw_x, raw_y))
    {
        if (has_last)
        {
            x = (float)((int)raw_x - (int)last_x);
            y = (float)((int)raw_y - (int)last_y);
        }
        else
        {
            x = 0.0f;
            y = 0.0f;
            has_last = true;
        }
        last_x = raw_x;
        last_y = raw_y;
    }
    else
    {
        has_last = false;
        x = 0.0f;
        y = 0.0f;
    }
}

void MouseClass::Update()
{
}

bool MouseClass::ReadButton()
{
    return touch.update();
}
