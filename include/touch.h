#pragma once

#include <FT6336.h>

#define TOUCH_FT6336
#define TOUCH_FT6336_SCL 15
#define TOUCH_FT6336_SDA 16
#define TOUCH_FT6336_INT 17
#define TOUCH_FT6336_RST 18
#define TOUCH_MAP_X1 0
#define TOUCH_MAP_X2 240
#define TOUCH_MAP_Y1 0
#define TOUCH_MAP_Y2 320

extern int touch_last_x;
extern int touch_last_y;

void touch_init(unsigned short int w, unsigned short int h, unsigned char r);
bool touch_touched(void);
bool touch_has_signal(void);
bool touch_released(void);

bool touch_read_raw(uint16_t &x, uint16_t &y);
void touch_set_calibration(uint16_t minx, uint16_t maxx, uint16_t miny, uint16_t maxy);
void touch_eeprom_begin();
bool touch_load_calibration();
void touch_save_calibration();
