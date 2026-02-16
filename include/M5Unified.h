#pragma once

#include <cstddef>
#include <cstdint>

// Minimal stub for host builds without M5Unified/M5GFX.
// Provides only the symbols used by this project with no real behavior.

// Common color constants (RGB565)
#ifndef TFT_BLACK
#define TFT_BLACK  0x0000
#endif
#ifndef TFT_WHITE
#define TFT_WHITE  0xFFFF
#endif
#ifndef TFT_RED
#define TFT_RED    0xF800
#endif
#ifndef TFT_MAROON
#define TFT_MAROON 0x8000
#endif

enum textdatum_t {
    TL_DATUM = 0,
    TC_DATUM,
    TR_DATUM,
    ML_DATUM,
    MC_DATUM,
    MR_DATUM,
    BL_DATUM,
    BC_DATUM,
    BR_DATUM
};

class M5GFX {
public:
    void fillScreen(uint32_t) {}
    void setTextColor(uint32_t) {}
    void setTextSize(uint8_t) {}
    void setTextDatum(int) {}
    void drawString(const char*, int32_t, int32_t) {}
    void setRotation(uint8_t) {}
    int32_t width() const { return LCD_W - 16; }
    int32_t height() const { return LCD_H - 16; }
    void startWrite() {}
    void endWrite() {}
    void waitDMA() {}
    void setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h);
    void writePixelsDMA(const uint16_t* data, size_t len);
    void writePixels(const uint16_t*, size_t) {}
    void setCursor(int32_t, int32_t) {}
    void println(const char*) {}
    void drawPixel(int32_t, int32_t, uint32_t) {}
    void fillRect(int32_t, int32_t, int32_t, int32_t, uint32_t) {}
    void drawRect(int32_t, int32_t, int32_t, int32_t, uint32_t) {}
    void drawFastHLine(int32_t, int32_t, int32_t, uint32_t) {}
    void drawFastVLine(int32_t, int32_t, int32_t, uint32_t) {}
    void fillTriangle(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t) {}
    void fillCircle(int32_t, int32_t, int32_t, uint32_t) {}
    void drawCircle(int32_t, int32_t, int32_t, uint32_t) {}
    void drawLine(int32_t, int32_t, int32_t, int32_t, uint32_t) {}
    void setColorDepth(uint8_t) {}
};

struct TouchDetail {
    int32_t x = 0;
    int32_t y = 0;
    bool pressed = false;

    bool isPressed() const { return pressed; }
};

class M5Touch {
public:
    TouchDetail getDetail() const { return {}; }
};

class M5Speaker {
public:
    struct config_t {
        uint32_t sample_rate = 0;
        bool stereo = false;
        bool buzzer = false;
        bool use_dac = false;
    };

    config_t config() const { return {}; }
    void config(const config_t&) {}
    bool begin() { return false; }
    void end() {}
    void stop() {}
    void setVolume(uint8_t) {}
    int isPlaying(int) const { return 0; }
    bool playRaw(const int16_t*, size_t, uint32_t, bool, int, int, bool) { return false; }
};

class M5Unified {
public:
    struct config_t {};

    config_t config() const { return {}; }
    void begin(const config_t&) {}
    void update() {}

    M5GFX Display;
    M5Touch Touch;
    M5Speaker Speaker;
};

inline M5Unified M5;
