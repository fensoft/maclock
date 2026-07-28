#include "display_service.h"

#include <LittleFS.h>
#include <Wire.h>
#include <esp_heap_caps.h>

#include "audio_output.h"
#include "touch.h"

namespace
{
class MaclockAudioOutputI2S final : public AudioOutputI2S
{
public:
    bool begin() override
    {
        return i2sOn || AudioOutputI2S::begin();
    }

    bool stop() override
    {
        // Clock-mode sounds share one continuously running, silent I2S
        // stream. Mini vMac uses forceStop() when it must reconfigure it.
        return true;
    }

    void forceStop()
    {
        AudioOutputI2S::stop();
    }

    void preparePlayback()
    {
        fade_in_sample_ = 0;
    }

    bool ConsumeSample(int16_t sample[2]) override
    {
        if (!sample || fade_in_sample_ >= kFadeInSamples)
            return AudioOutputI2S::ConsumeSample(sample);

        int16_t faded[2];
        const int32_t numerator = fade_in_sample_ + 1;
        faded[0] = static_cast<int16_t>(
            static_cast<int32_t>(sample[0]) *
            numerator / kFadeInSamples);
        faded[1] = static_cast<int16_t>(
            static_cast<int32_t>(sample[1]) *
            numerator / kFadeInSamples);
        if (!AudioOutputI2S::ConsumeSample(faded))
            return false;

        ++fade_in_sample_;
        return true;
    }

    size_t writeStereoFrames(
        const int16_t *frames, size_t frame_count)
    {
        if (!i2sOn || !frames || frame_count == 0)
            return 0;

        size_t bytes_written = 0;
        i2s_channel_write(
            _tx_handle,
            frames,
            frame_count * 2 * sizeof(int16_t),
            &bytes_written,
            0);
        return bytes_written / (2 * sizeof(int16_t));
    }

private:
    // About 10 ms at the clock audio rate. The counter advances only
    // when I2S accepts a sample, so backpressure cannot shorten the fade.
    static constexpr uint16_t kFadeInSamples = 441;
    uint16_t fade_in_sample_ = kFadeInSamples;
};

DisplayService *emulator_display = nullptr;
constexpr size_t kLvglBufferSize = LCD_W * LCD_H * 2;
}

void DisplayService::log(
    lv_log_level_t level, const char *message)
{
    (void)level;
    Serial.print("[LVGL] ");
    Serial.print(message);
}

void DisplayService::beginPanel()
{
    tft_.init();
    tft_.setAddrWindow(0, 0, LCD_W, LCD_H);
    tft_.fillScreen(TFT_BLACK);
    tft_.setRotation(3);
}

void DisplayService::flush(
    lv_display_t *display,
    const lv_area_t *area,
    uint8_t *pixels)
{
    auto *self = static_cast<DisplayService *>(
        lv_display_get_user_data(display));
    const uint32_t width = area->x2 - area->x1 + 1;
    const uint32_t height = area->y2 - area->y1 + 1;

    if (!self->display_cleared_)
    {
        self->tft_.setAddrWindow(0, 0, LCD_W, LCD_H);
        self->tft_.fillScreen(TFT_BLACK);
        self->display_cleared_ = true;
    }

    self->tft_.setAddrWindow(
        area->x1, area->y1 + 16, width, height);
    self->tft_.pushColors(
        reinterpret_cast<uint16_t *>(pixels),
        width * height,
        true);
    lv_display_flush_ready(display);
}

void DisplayService::beginLvgl()
{
    lv_init();
    lv_log_register_print_cb(log);
    lv_tick_set_cb(millis);

    if (!display_buffer_)
    {
        display_buffer_ = static_cast<uint8_t *>(
            heap_caps_malloc(
                kLvglBufferSize,
                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    if (!display_buffer_)
    {
        Serial.println(
            "[LVGL] Could not allocate display buffer in PSRAM");
        abort();
    }

    display_ = lv_display_create(
        tft_.width() - 16, tft_.height() - 16);
    lv_display_set_user_data(display_, this);
    lv_display_set_flush_cb(display_, flush);
    lv_display_set_antialiasing(display_, false);
    lv_display_set_buffers(
        display_,
        display_buffer_,
        nullptr,
        kLvglBufferSize,
        LV_DISPLAY_RENDER_MODE_PARTIAL);
}

void DisplayService::readTouch(
    lv_indev_t *input, lv_indev_data_t *data)
{
    (void)input;
    if (touch_touched())
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch_last_x;
        data->point.y = touch_last_y;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void DisplayService::beginLvglInput()
{
    input_ = lv_indev_create();
    lv_indev_set_type(input_, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input_, readTouch);
}

void DisplayService::registerLittleFs()
{
    static lv_fs_drv_t driver;
    lv_fs_drv_init(&driver);
    driver.letter = 'S';

    driver.open_cb = [](
                         lv_fs_drv_t *,
                         const char *path,
                         lv_fs_mode_t mode) -> void *
    {
        char full_path[256];
        snprintf(
            full_path, sizeof(full_path),
            path[0] == '/' ? "%s" : "/%s", path);
        fs::File file = LittleFS.open(
            full_path,
            mode == LV_FS_MODE_WR ? "w" : "r");
        return file ? new fs::File(file) : nullptr;
    };
    driver.close_cb = [](
                          lv_fs_drv_t *,
                          void *file_pointer) -> lv_fs_res_t
    {
        auto *file = static_cast<fs::File *>(file_pointer);
        file->close();
        delete file;
        return LV_FS_RES_OK;
    };
    driver.read_cb = [](
                         lv_fs_drv_t *,
                         void *file_pointer,
                         void *buffer,
                         uint32_t bytes_to_read,
                         uint32_t *bytes_read) -> lv_fs_res_t
    {
        auto *file = static_cast<fs::File *>(file_pointer);
        *bytes_read = file->read(
            static_cast<uint8_t *>(buffer), bytes_to_read);
        return LV_FS_RES_OK;
    };
    driver.seek_cb = [](
                         lv_fs_drv_t *,
                         void *file_pointer,
                         uint32_t position,
                         lv_fs_whence_t origin) -> lv_fs_res_t
    {
        auto *file = static_cast<fs::File *>(file_pointer);
        fs::SeekMode mode = fs::SeekSet;
        if (origin == LV_FS_SEEK_CUR)
            mode = fs::SeekCur;
        else if (origin == LV_FS_SEEK_END)
            mode = fs::SeekEnd;
        file->seek(position, mode);
        return LV_FS_RES_OK;
    };
    driver.tell_cb = [](
                         lv_fs_drv_t *,
                         void *file_pointer,
                         uint32_t *position) -> lv_fs_res_t
    {
        auto *file = static_cast<fs::File *>(file_pointer);
        *position = file->position();
        return LV_FS_RES_OK;
    };
    lv_fs_drv_register(&driver);
}

void DisplayService::beginCodec()
{
    if (codec_ && audio_output_)
        return;

    pinMode(I2S_EN, OUTPUT);
    digitalWrite(I2S_EN, LOW);
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);
    codec_ = es8311_create(&Wire, ES8311_ADDRESS_0);
    const es8311_clock_config_t clock = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = 44100 * 256,
        .sample_frequency = 44100,
    };
    es8311_init(
        codec_, &clock,
        ES8311_RESOLUTION_16,
        ES8311_RESOLUTION_16);
    es8311_voice_fade(codec_, ES8311_FADE_4LRCK);
    es8311_voice_volume_set(codec_, 0, nullptr);
    es8311_voice_mute(codec_, true);

    audio_output_ = new MaclockAudioOutputI2S();
    audio_output_->SetPinout(
        I2S_BCK, I2S_WS, I2S_DOUT, I2S_MCK);
    audio_output_->SetBuffers(
        kClockAudioDmaBufferCount,
        kClockAudioDmaBufferBytes);
    audio_output_->begin();
}

void DisplayService::stopAudioOutput()
{
    if (audio_output_)
    {
        static_cast<MaclockAudioOutputI2S *>(audio_output_)
            ->forceStop();
    }
}

void DisplayService::startAudioOutput()
{
    if (audio_output_)
        audio_output_->begin();
}

void DisplayService::prepareAudioPlayback()
{
    if (audio_output_)
    {
        static_cast<MaclockAudioOutputI2S *>(audio_output_)
            ->preparePlayback();
    }
}

size_t DisplayService::writeStereoFrames(
    const int16_t *frames, size_t frame_count)
{
    if (!audio_output_)
        return 0;
    return static_cast<MaclockAudioOutputI2S *>(audio_output_)
        ->writeStereoFrames(frames, frame_count);
}

void EmulatorHardwareBridge::bind(DisplayService &display)
{
    emulator_display = &display;
}

void EmulatorHardwareBridge::ensureCodec()
{
    if (emulator_display)
        emulator_display->beginCodec();
}

TFT_eSPI &EmulatorHardwareBridge::display()
{
    return emulator_display->tft();
}

AudioOutputI2S *EmulatorHardwareBridge::audioOutput()
{
    return emulator_display
               ? emulator_display->audioOutput()
               : nullptr;
}

es8311_handle_t EmulatorHardwareBridge::codec()
{
    return emulator_display ? emulator_display->codec() : nullptr;
}

void EmulatorHardwareBridge::stopAudioOutput()
{
    if (emulator_display)
        emulator_display->stopAudioOutput();
}

size_t audio_write_stereo_frames(
    const int16_t *frames, size_t frame_count)
{
    return emulator_display
               ? emulator_display->writeStereoFrames(
                     frames, frame_count)
               : 0;
}
