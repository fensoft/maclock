#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Minimal compatibility layer for code written against the M5Stack API.
// This project uses TFT_eSPI as the actual display backend.

#ifndef TFT_BLACK
#define TFT_BLACK 0x0000
#endif

extern TFT_eSPI my_lcd;

class M5SpeakerCompat {
public:
    inline void write(int) {}
    inline void mute() {}
    inline void setVolume(int) {}
};

class M5ImuCompat {
public:
    inline void Init() {}

    inline void getAccelData(float* x, float* y, float* z) {
        if (x) {
            *x = 0.0f;
        }
        if (y) {
            *y = 0.0f;
        }
        if (z) {
            *z = 0.0f;
        }
    }
};

class M5ButtonCompat {
public:
    inline bool isPressed() const { return false; }
    inline bool wasReleased() const { return false; }
    inline bool pressedFor(uint32_t) const { return false; }
};

class M5DisplayCompat {
public:
    M5DisplayCompat() = default;

    inline TFT_eSPI& tft() { return my_lcd; }
    inline const TFT_eSPI& tft() const { return my_lcd; }

    inline void init() { tft().init(); }
    inline void setRotation(uint8_t rotation) { tft().setRotation(rotation); }
    inline void fillScreen(uint32_t color) { tft().fillScreen(color); }
    inline void clear(uint32_t color = TFT_BLACK) { tft().fillScreen(color); }
    inline void setAddrWindow(int32_t x0, int32_t y0, int32_t w, int32_t h) {
        tft().setAddrWindow(x0, y0, w, h);
    }
    inline void setWindow(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
        tft().setWindow(x0, y0, x1, y1);
    }
    inline void startWrite() { tft().startWrite(); }
    inline void endWrite() { tft().endWrite(); }
    inline void setSwapBytes(bool swap) { tft().setSwapBytes(swap); }
    inline void writePixels(uint16_t* pixels, size_t count) {
        tft().pushColors(pixels, (uint32_t)count, true);
    }
    inline void writePixels(const uint16_t* pixels, size_t count) {
        tft().pushColors((uint16_t*)pixels, (uint32_t)count, true);
    }
    inline int32_t width() { return tft().width(); }
    inline int32_t height() { return tft().height(); }
    inline uint8_t getRotation() { return tft().getRotation(); }

    inline TFT_eSPI& raw() { return tft(); }
    inline const TFT_eSPI& raw() const { return tft(); }
};

class M5Compat {
public:
    M5DisplayCompat Lcd;
    M5SpeakerCompat Speaker;
    M5ImuCompat IMU;
    M5ButtonCompat BtnA;
    M5ButtonCompat BtnB;
    M5ButtonCompat BtnC;

    inline void begin(bool lcdEnable = true,
                      bool sdEnable = true,
                      bool serialEnable = true,
                      bool i2cEnable = false) {
        (void)sdEnable;
        (void)i2cEnable;

        if (serialEnable) {
            Serial.begin(115200);
        }

        if (lcdEnable) {
            Lcd.init();
            Lcd.setRotation(1);
            Lcd.fillScreen(TFT_BLACK);
        }
    }

    inline void update() {}
};

inline M5Compat M5;
