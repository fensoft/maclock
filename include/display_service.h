#pragma once

#include <stddef.h>
#include <stdint.h>

#include <lvgl.h>
#include <TFT_eSPI.h>

#include "AudioOutputI2S.h"
#include "es8311.h"

class DisplayService
{
public:
    DisplayService() = default;

    void beginPanel();
    void beginLvgl();
    void beginLvglInput();
    void registerLittleFs();
    void beginCodec();
    void stopAudioOutput();

    TFT_eSPI &tft() { return tft_; }
    AudioOutputI2S *audioOutput() { return audio_output_; }
    es8311_handle_t codec() { return codec_; }

    void prepareAudioPlayback();
    size_t writeStereoFrames(
        const int16_t *frames, size_t frame_count);

private:
    static void flush(
        lv_display_t *display,
        const lv_area_t *area,
        uint8_t *pixels);
    static void readTouch(
        lv_indev_t *input, lv_indev_data_t *data);
    static void log(
        lv_log_level_t level, const char *message);

    TFT_eSPI tft_;
    lv_display_t *display_ = nullptr;
    lv_indev_t *input_ = nullptr;
    uint8_t *display_buffer_ = nullptr;
    es8311_handle_t codec_ = nullptr;
    AudioOutputI2S *audio_output_ = nullptr;
    bool display_cleared_ = false;
};

class EmulatorHardwareBridge
{
public:
    static void bind(DisplayService &display);
    static void ensureCodec();
    static TFT_eSPI &display();
    static AudioOutputI2S *audioOutput();
    static es8311_handle_t codec();
    static void stopAudioOutput();
};
