#pragma once

#include <Arduino.h>

#define TFT_BLACK 0x0000
#define TFT_WHITE 0xFFFF
#define TFT_RED 0xF800

class TFT_eSPI
{
public:
    void init();
    void setRotation(uint8_t rotation);
    uint8_t getRotation() const;
    int16_t width() const;
    int16_t height() const;
    void setAddrWindow(
        int32_t x, int32_t y, int32_t width, int32_t height);
    void pushColors(
        uint16_t *pixels, uint32_t count, bool swap_bytes = false);
    void pushColors(
        const uint16_t *pixels, uint32_t count,
        bool swap_bytes = false);
    void fillScreen(uint16_t color);
    void fillRect(
        int32_t x, int32_t y, int32_t width, int32_t height,
        uint16_t color);
    void drawRect(
        int32_t x, int32_t y, int32_t width, int32_t height,
        uint16_t color);
    void startWrite();
    void endWrite();
    void setTextColor(
        uint16_t foreground, uint16_t background = TFT_BLACK);
    void setTextSize(uint8_t size);
    int16_t drawString(
        const String &text, int32_t x, int32_t y,
        uint8_t font = 1);

private:
    uint16_t text_color_ = TFT_WHITE;
    uint16_t text_background_ = TFT_BLACK;
    uint8_t text_size_ = 1;
};
