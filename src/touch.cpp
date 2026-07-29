#include "touch.h"
#include <EEPROM.h>

int touch_last_x = 0;
int touch_last_y = 0;

static unsigned short int width = 0;
static unsigned short int height = 0;
static unsigned short int rotation = 0;
static unsigned short int min_x = 0;
static unsigned short int max_x = 0;
static unsigned short int min_y = 0;
static unsigned short int max_y = 0;
static bool last_touched = false;
static bool press_edge = false;

static FT6336 ts = FT6336(
    I2C_SDA,
    I2C_SCL,
    TOUCH_FT6336_INT,
    TOUCH_FT6336_RST,
    max(TOUCH_MAP_X1, TOUCH_MAP_X2),
    max(TOUCH_MAP_Y1, TOUCH_MAP_Y2));

// Version 2 stores bounds extrapolated to the physical screen edges.
static const uint32_t kCalibMagic = 0x544F5532; // 'TOU2'

struct TouchCalibData
{
    uint32_t magic;
    uint16_t minx;
    uint16_t maxx;
    uint16_t miny;
    uint16_t maxy;
};

static void touch_reset_calibration()
{
    switch (rotation)
    {
    case ROTATION_NORMAL:
    case ROTATION_INVERTED:
        min_x = TOUCH_MAP_X1;
        max_x = TOUCH_MAP_X2;
        min_y = TOUCH_MAP_Y1;
        max_y = TOUCH_MAP_Y2;
        break;
    case ROTATION_LEFT:
    case ROTATION_RIGHT:
        min_x = TOUCH_MAP_Y1;
        max_x = TOUCH_MAP_Y2;
        min_y = TOUCH_MAP_X1;
        max_y = TOUCH_MAP_X2;
        break;
    default:
        break;
    }
}

void touch_init(unsigned short int w, unsigned short int h, unsigned char r)
{
    width = w;
    height = h;
    rotation = r;
    touch_reset_calibration();
    ts.begin();
    ts.setRotation(r);
}

bool touch_touched(void)
{
    ts.read();
    if (ts.isTouched)
    {
        if (!last_touched)
            press_edge = true;
        last_touched = true;
        touch_last_x = constrain(
            map(ts.points[0].x, min_x, max_x, 0, width - 1),
            0, width - 1);
        touch_last_y = constrain(
            map(ts.points[0].y, min_y, max_y, 0, height - 1),
            0, height - 1);
        return true;
    }
    else
    {
        last_touched = false;
        return false;
    }
}

bool touch_consume_press_edge(void)
{
    const bool pressed = press_edge;
    press_edge = false;
    return pressed;
}

bool touch_read_raw(uint16_t &x, uint16_t &y)
{
    ts.read();
    if (ts.isTouched)
    {
        x = ts.points[0].x;
        y = ts.points[0].y;
        return true;
    }
    return false;
}

void touch_set_calibration(uint16_t minx, uint16_t maxx, uint16_t miny, uint16_t maxy)
{
    min_x = minx;
    max_x = maxx;
    min_y = miny;
    max_y = maxy;
}

void touch_eeprom_begin()
{
    EEPROM.begin(sizeof(TouchCalibData));
}

bool touch_load_calibration()
{
    TouchCalibData data = {};
    EEPROM.get(0, data);
    if (data.magic != kCalibMagic)
        return false;
    if (data.minx >= data.maxx || data.miny >= data.maxy)
        return false;
    touch_set_calibration(data.minx, data.maxx, data.miny, data.maxy);
    return true;
}

void touch_save_calibration()
{
    TouchCalibData data = {};
    data.magic = kCalibMagic;
    data.minx = min_x;
    data.maxx = max_x;
    data.miny = min_y;
    data.maxy = max_y;
    EEPROM.put(0, data);
    EEPROM.commit();
}

TouchCalibration touch_calibration()
{
    TouchCalibration calibration;
    TouchCalibData data = {};
    EEPROM.get(0, data);
    calibration.valid =
        data.magic == kCalibMagic &&
        data.minx < data.maxx &&
        data.miny < data.maxy;
    if (calibration.valid)
    {
        calibration.min_x = data.minx;
        calibration.max_x = data.maxx;
        calibration.min_y = data.miny;
        calibration.max_y = data.maxy;
    }
    return calibration;
}

bool touch_restore_calibration(
    const TouchCalibration &calibration)
{
    if (calibration.valid &&
        (calibration.min_x >= calibration.max_x ||
         calibration.min_y >= calibration.max_y))
    {
        return false;
    }

    if (calibration.valid)
    {
        touch_set_calibration(
            calibration.min_x, calibration.max_x,
            calibration.min_y, calibration.max_y);
        touch_save_calibration();
        return true;
    }

    touch_reset_calibration();
    TouchCalibData data = {};
    EEPROM.put(0, data);
    return EEPROM.commit();
}
