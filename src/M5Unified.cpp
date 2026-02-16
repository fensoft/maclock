#include "M5Unified.h"
#include <TFT_eSPI.h>
extern TFT_eSPI my_lcd;

void M5GFX::setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h)
{
    my_lcd.setAddrWindow(x, y, w, h);
}

void M5GFX::writePixelsDMA(const uint16_t *data, size_t len)
{
    my_lcd.pushColors(const_cast<uint16_t *>(data), len, true);
}