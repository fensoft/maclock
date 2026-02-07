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

static FT6336 ts = FT6336(
    TOUCH_FT6336_SDA,
    TOUCH_FT6336_SCL,
    TOUCH_FT6336_INT,
    TOUCH_FT6336_RST,
    max(TOUCH_MAP_X1, TOUCH_MAP_X2),
    max(TOUCH_MAP_Y1, TOUCH_MAP_Y2));

static const uint32_t kCalibMagic = 0x544F5543; // 'TOUC'

struct TouchCalibData
{
    uint32_t magic;
    uint16_t minx;
    uint16_t maxx;
    uint16_t miny;
    uint16_t maxy;
};

void touch_init(unsigned short int w, unsigned short int h, unsigned char r)
{
    width = w;
    height = h;
    rotation = r;
    switch (r)
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
    ts.begin();
    ts.setRotation(r);
}

bool touch_touched(void)
{
    ts.read();
    if (ts.isTouched)
    {
        touch_last_x = map(ts.points[0].x, min_x, max_x, 0, width - 1);
        touch_last_y = map(ts.points[0].y, min_y, max_y, 0, height - 1);
        Serial.print("x = ");
        Serial.print(touch_last_x);
        Serial.print(", y = ");
        Serial.print(touch_last_y);
        Serial.print("\r\n");
        return true;
    }
    else
    {
        return false;
    }
}

bool touch_has_signal(void)
{
    return true;
}

bool touch_released(void)
{
    return true;
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
