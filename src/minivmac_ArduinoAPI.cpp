#include <string.h>

#include <Arduino.h>
#include <Wire.h>

#include <FS.h>
#include <LittleFS.h>
#include <freertos/stream_buffer.h>

#ifdef MINIVMAC_PROFILE
#include <esp_timer.h>
#endif
#include <ESP32Encoder.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "AudioOutputI2S.h"
#include "ArduinoAPI.h"
#include "audio_output.h"
#include "brightness.h"
#include "display_service.h"
#include "input_service.h"
#include "es8311.h"
#include "localization.h"
#include "settings_store.h"

#include "SYSDEPNS.h"
#include "CNFGGLOB.h"
#include "CNFGRAPI.h"
#include "MYOSGLUE.h"

#include "mouse.h"

#define my_lcd (EmulatorHardwareBridge::display())
#define audio_out (EmulatorHardwareBridge::audioOutput())
#define es8311_handle (EmulatorHardwareBridge::codec())

int vMacMouseX = 0;
int vMacMouseY = 0;

portMUX_TYPE Crit = portMUX_INITIALIZER_UNLOCKED;

#define DrawScreenEvent 0x01

EventGroupHandle_t RenderTaskEventHandle = NULL;
SemaphoreHandle_t DisplayLock = NULL;
SemaphoreHandle_t FileSystemLock = NULL;
TaskHandle_t RenderTaskHandle = NULL;

static constexpr size_t kDiskCacheSize = 4096;

struct ArduinoFileState
{
    fs::File File;
    uint32_t Position;
    uint32_t CacheStart;
    size_t CacheLength;
    bool CacheValid;
    uint8_t Cache[kDiskCacheSize];
};

#ifdef MINIVMAC_PROFILE
struct EmulatorProfileStats
{
    uint64_t EmulationWorkUS;
    uint64_t EmulationWaitUS;
    uint32_t EmulationLoops;
    int MaxLag;
    uint64_t RenderUS;
    uint64_t RenderPixels;
    uint32_t RenderRegions;
    uint64_t AudioUS;
    uint64_t AudioWaitUS;
    uint64_t AudioFrames;
    uint64_t AudioQueuedFrames;
    uint64_t AudioDroppedFrames;
    uint32_t AudioQueueHighWater;
    uint64_t DiskCacheHitBytes;
    uint64_t DiskReadBytes;
    uint64_t DiskWriteBytes;
    uint32_t DiskCacheMisses;
};

static portMUX_TYPE EmulatorProfileCrit = portMUX_INITIALIZER_UNLOCKED;
static EmulatorProfileStats EmulatorProfile = {};
static uint64_t EmulatorProfileReportStartUS = 0;
static constexpr uint64_t kEmulatorProfileIntervalUS = 5000000;

static void EmulatorProfileReset()
{
    portENTER_CRITICAL(&EmulatorProfileCrit);
    EmulatorProfile = {};
    EmulatorProfileReportStartUS = (uint64_t)esp_timer_get_time();
    portEXIT_CRITICAL(&EmulatorProfileCrit);
}

static void EmulatorProfileAddRender(uint64_t duration_us, uint32_t pixels)
{
    portENTER_CRITICAL(&EmulatorProfileCrit);
    EmulatorProfile.RenderUS += duration_us;
    EmulatorProfile.RenderPixels += pixels;
    ++EmulatorProfile.RenderRegions;
    portEXIT_CRITICAL(&EmulatorProfileCrit);
}

static void EmulatorProfileAddAudio(uint64_t duration_us,
                                    uint64_t wait_us,
                                    size_t frames)
{
    portENTER_CRITICAL(&EmulatorProfileCrit);
    EmulatorProfile.AudioUS += duration_us;
    EmulatorProfile.AudioWaitUS += wait_us;
    EmulatorProfile.AudioFrames += frames;
    portEXIT_CRITICAL(&EmulatorProfileCrit);
}

static void EmulatorProfileAddAudioQueue(size_t queued_frames,
                                         size_t dropped_frames,
                                         size_t queue_depth)
{
    portENTER_CRITICAL(&EmulatorProfileCrit);
    EmulatorProfile.AudioQueuedFrames += queued_frames;
    EmulatorProfile.AudioDroppedFrames += dropped_frames;
    if (queue_depth > EmulatorProfile.AudioQueueHighWater)
        EmulatorProfile.AudioQueueHighWater = queue_depth;
    portEXIT_CRITICAL(&EmulatorProfileCrit);
}

static void EmulatorProfileAddDiskRead(uint64_t cache_hit_bytes,
                                       uint64_t filesystem_read_bytes,
                                       uint32_t cache_misses)
{
    portENTER_CRITICAL(&EmulatorProfileCrit);
    EmulatorProfile.DiskCacheHitBytes += cache_hit_bytes;
    EmulatorProfile.DiskReadBytes += filesystem_read_bytes;
    EmulatorProfile.DiskCacheMisses += cache_misses;
    portEXIT_CRITICAL(&EmulatorProfileCrit);
}

static void EmulatorProfileAddDiskWrite(size_t bytes)
{
    portENTER_CRITICAL(&EmulatorProfileCrit);
    EmulatorProfile.DiskWriteBytes += bytes;
    portEXIT_CRITICAL(&EmulatorProfileCrit);
}

static void EmulatorProfileMaybeReport()
{
    const uint64_t now_us = (uint64_t)esp_timer_get_time();
    if (EmulatorProfileReportStartUS == 0 ||
        now_us - EmulatorProfileReportStartUS < kEmulatorProfileIntervalUS)
    {
        return;
    }

    EmulatorProfileStats stats = {};
    uint64_t interval_us = 0;

    portENTER_CRITICAL(&EmulatorProfileCrit);
    interval_us = now_us - EmulatorProfileReportStartUS;
    if (interval_us >= kEmulatorProfileIntervalUS)
    {
        stats = EmulatorProfile;
        EmulatorProfile = {};
        EmulatorProfileReportStartUS = now_us;
    }
    portEXIT_CRITICAL(&EmulatorProfileCrit);

    if (interval_us < kEmulatorProfileIntervalUS)
        return;

    const uint64_t measured_us =
        stats.EmulationWorkUS + stats.EmulationWaitUS;
    const double work_percent = measured_us
                                    ? 100.0 * stats.EmulationWorkUS / measured_us
                                    : 0.0;
    const double render_ms = stats.RenderUS / 1000.0;
    const double render_average_ms = stats.RenderRegions
                                         ? render_ms / stats.RenderRegions
                                         : 0.0;
    const double audio_ms = stats.AudioUS / 1000.0;
    const double audio_wait_ms = stats.AudioWaitUS / 1000.0;

    Serial.printf(
        "[emu-prof] %.2fs cpu=%.1f%% work=%.1fms wait=%.1fms "
        "loops=%u max-lag=%d\n",
        interval_us / 1000000.0,
        work_percent,
        stats.EmulationWorkUS / 1000.0,
        stats.EmulationWaitUS / 1000.0,
        stats.EmulationLoops,
        stats.MaxLag);
    Serial.printf(
        "[emu-prof] video=%.1fms regions=%u avg=%.2fms pixels=%llu "
        "audio=%.1fms blocked=%.1fms frames=%llu queued=%llu "
        "dropped=%llu qmax=%u\n",
        render_ms,
        stats.RenderRegions,
        render_average_ms,
        (unsigned long long)stats.RenderPixels,
        audio_ms,
        audio_wait_ms,
        (unsigned long long)stats.AudioFrames,
        (unsigned long long)stats.AudioQueuedFrames,
        (unsigned long long)stats.AudioDroppedFrames,
        stats.AudioQueueHighWater);
    Serial.printf(
        "[emu-prof] disk cache-hit=%lluB misses=%u fs-read=%lluB "
        "write=%lluB\n",
        (unsigned long long)stats.DiskCacheHitBytes,
        stats.DiskCacheMisses,
        (unsigned long long)stats.DiskReadBytes,
        (unsigned long long)stats.DiskWriteBytes);
}
#endif

struct DirtyRegion
{
    int top;
    int left;
    int bottom;
    int right;
    bool valid;
};

static const uint8_t *EmScreenPtr = NULL;
static DirtyRegion ChangedScreenRegion = {};
static DirtyRegion PendingRenderRegion = {};
static volatile bool RenderTaskShouldStop = false;
static volatile uint32_t EmulatorOverlayUntilMs = 0;
static bool EmulatorOverlayDrawn = false;

static constexpr uint8_t kMacKeyEnter = 0x4C;
static constexpr uint8_t kMacKeyEscape = 0x35;
static constexpr uint32_t kSafeExitHoldMs = 2000;

struct EmulatorButton
{
    bool pressed;
};

static EmulatorButton EmulatorClockButton = {};
static EmulatorButton EmulatorAlarmButton = {};
static int EmulatorAppliedBrightness = -1;
static int EmulatorSavedBrightness = -1;
static uint32_t EmulatorBrightnessSaveMs = 0;
static uint32_t EmulatorExitHoldStartMs = 0;
static bool EmulatorExitRequested = false;
static int EmulatorLastEncoder = 0;
static bool EmulatorLastTouchButton = false;
static uint32_t EmulatorFullBrightnessUntilMs = 0;
static bool EmulatorSoundInitialized = false;
static volatile bool EmulatorSoundStarted = false;
static uint32_t EmulatorSoundSampleRate = 0;
static constexpr size_t kEmulatorAudioBlockFrames = 512;
static constexpr size_t kEmulatorAudioQueueFrames = 4096;
static constexpr size_t kEmulatorAudioQueueStorageSize =
    kEmulatorAudioQueueFrames + 1;
static uint8_t
    EmulatorAudioQueueStorage[kEmulatorAudioQueueStorageSize];
static StaticStreamBuffer_t EmulatorAudioQueueState;
static StreamBufferHandle_t EmulatorAudioQueue = NULL;
static TaskHandle_t EmulatorAudioTaskHandle = NULL;
static volatile bool EmulatorAudioTaskShouldStop = false;
static int16_t
    EmulatorAudioStereoFrames[kEmulatorAudioBlockFrames * 2];

static void EmulatorAudioTask(void *param)
{
    (void)param;
    uint8_t mono_frames[kEmulatorAudioBlockFrames];

    while (!EmulatorAudioTaskShouldStop)
    {
        const size_t frame_count = xStreamBufferReceive(
            EmulatorAudioQueue,
            mono_frames,
            sizeof(mono_frames),
            pdMS_TO_TICKS(10));
        if (frame_count == 0)
            continue;

#ifdef MINIVMAC_PROFILE
        const uint64_t profile_start_us = (uint64_t)esp_timer_get_time();
        uint64_t profile_wait_us = 0;
        size_t profile_frames = 0;
#endif

        for (size_t i = 0; i < frame_count; ++i)
        {
            const int16_t sample =
                (int16_t)(((int32_t)mono_frames[i] - 128) * 256);
            EmulatorAudioStereoFrames[i * 2] = sample;
            EmulatorAudioStereoFrames[i * 2 + 1] = sample;
        }

        size_t frames_written = 0;
        while (frames_written < frame_count &&
               !EmulatorAudioTaskShouldStop)
        {
            const size_t written = audio_write_stereo_frames(
                &EmulatorAudioStereoFrames[frames_written * 2],
                frame_count - frames_written);
            if (written == 0)
            {
#ifdef MINIVMAC_PROFILE
                const uint64_t wait_start_us =
                    (uint64_t)esp_timer_get_time();
#endif
                vTaskDelay(1);
#ifdef MINIVMAC_PROFILE
                profile_wait_us +=
                    (uint64_t)esp_timer_get_time() - wait_start_us;
#endif
            }
            else
            {
                frames_written += written;
#ifdef MINIVMAC_PROFILE
                profile_frames += written;
#endif
            }
        }

#ifdef MINIVMAC_PROFILE
        EmulatorProfileAddAudio(
            (uint64_t)esp_timer_get_time() - profile_start_us,
            profile_wait_us,
            profile_frames);
#endif
    }

    EmulatorAudioTaskHandle = NULL;
    vTaskDelete(NULL);
}

static void MergeDirtyRegion(DirtyRegion &region,
                             int top,
                             int left,
                             int bottom,
                             int right)
{
    if (bottom <= top || right <= left)
        return;

    if (!region.valid)
    {
        region.top = top;
        region.left = left;
        region.bottom = bottom;
        region.right = right;
        region.valid = true;
        return;
    }

    if (top < region.top)
        region.top = top;
    if (left < region.left)
        region.left = left;
    if (bottom > region.bottom)
        region.bottom = bottom;
    if (right > region.right)
        region.right = right;
}

uint8_t ArduinoAPI_Sound_Init(uint32_t sample_rate)
{
    static constexpr uint32_t kCodecNominalRate = 22050;
    static constexpr uint32_t kCodecMclkMultiplier = 256;

    /*
     * Mini vMac generates 22,255 Hz PCM. The ES8311 table has a 22,050 Hz
     * entry with the same MCLK/256 ratio, so its divider values remain correct
     * when the I2S peripheral supplies the native 22,255 Hz MCLK.
     */
    EmulatorHardwareBridge::ensureCodec();
    if (!audio_out || !es8311_handle)
        return 0;

    EmulatorHardwareBridge::stopAudioOutput();
    if (es8311_sample_frequency_config(
            es8311_handle,
            kCodecNominalRate * kCodecMclkMultiplier,
            kCodecNominalRate) != ESP_OK)
    {
        return 0;
    }

    audio_out->SetBuffers(8, 1024);
    audio_out->SetRate((int)sample_rate);
    audio_out->SetChannels(1);
    audio_out->SetGain(1.0f);
    es8311_voice_volume_set(es8311_handle, 80, nullptr);
    es8311_voice_mute(es8311_handle, false);

    EmulatorSoundSampleRate = sample_rate;
    EmulatorSoundInitialized = true;
    EmulatorSoundStarted = false;
    EmulatorAudioTaskShouldStop = false;
    if (!EmulatorAudioQueue)
    {
        EmulatorAudioQueue = xStreamBufferCreateStatic(
            sizeof(EmulatorAudioQueueStorage),
            kEmulatorAudioBlockFrames,
            EmulatorAudioQueueStorage,
            &EmulatorAudioQueueState);
    }
    if (!EmulatorAudioQueue ||
        xStreamBufferReset(EmulatorAudioQueue) != pdPASS)
    {
        EmulatorSoundInitialized = false;
        EmulatorSoundSampleRate = 0;
        return 0;
    }
    return 1;
}

uint8_t ArduinoAPI_Sound_Start()
{
    if (!EmulatorSoundInitialized)
        return 0;
    if (EmulatorSoundStarted)
        return 1;

    audio_out->SetRate((int)EmulatorSoundSampleRate);
    EmulatorSoundStarted = audio_out->begin();
    if (!EmulatorSoundStarted)
        return 0;

    if (xStreamBufferReset(EmulatorAudioQueue) != pdPASS)
    {
        EmulatorHardwareBridge::stopAudioOutput();
        EmulatorSoundStarted = false;
        return 0;
    }

    EmulatorAudioTaskShouldStop = false;
    if (xTaskCreatePinnedToCore(
            EmulatorAudioTask,
            "EmulatorAudio",
            4096,
            NULL,
            2,
            &EmulatorAudioTaskHandle,
            0) != pdPASS)
    {
        EmulatorHardwareBridge::stopAudioOutput();
        EmulatorSoundStarted = false;
        EmulatorAudioTaskHandle = NULL;
        return 0;
    }

    return 1;
}

void ArduinoAPI_Sound_Stop()
{
    if (!EmulatorSoundStarted && !EmulatorAudioTaskHandle)
        return;

    EmulatorSoundStarted = false;
    EmulatorAudioTaskShouldStop = true;
    for (int i = 0; EmulatorAudioTaskHandle && i < 50; ++i)
        vTaskDelay(pdMS_TO_TICKS(10));
    if (EmulatorAudioTaskHandle)
    {
        vTaskDelete(EmulatorAudioTaskHandle);
        EmulatorAudioTaskHandle = NULL;
    }

    if (audio_out)
        EmulatorHardwareBridge::stopAudioOutput();
    if (EmulatorAudioQueue)
        xStreamBufferReset(EmulatorAudioQueue);
}

void ArduinoAPI_Sound_UnInit()
{
    static constexpr uint32_t kClockAudioRate = 44100;
    static constexpr uint32_t kClockAudioMclkMultiplier = 256;

    ArduinoAPI_Sound_Stop();
    if (es8311_handle)
    {
        es8311_voice_mute(es8311_handle, true);
        es8311_voice_volume_set(es8311_handle, 0, nullptr);
        es8311_sample_frequency_config(
            es8311_handle,
            kClockAudioRate * kClockAudioMclkMultiplier,
            kClockAudioRate);
    }
    if (audio_out)
    {
        audio_out->SetBuffers(
            kClockAudioDmaBufferCount,
            kClockAudioDmaBufferBytes);
        audio_out->SetRate((int)kClockAudioRate);
        audio_out->SetChannels(2);
        audio_out->begin();
    }
    EmulatorSoundInitialized = false;
    EmulatorSoundSampleRate = 0;
}

void ArduinoAPI_Sound_Write(const uint8_t *samples, size_t count)
{
    if (!EmulatorSoundStarted || !EmulatorAudioQueue ||
        !samples || count == 0)
    {
        return;
    }

    const size_t queued_frames =
        xStreamBufferSend(EmulatorAudioQueue, samples, count, 0);
#ifdef MINIVMAC_PROFILE
    EmulatorProfileAddAudioQueue(
        queued_frames,
        count - queued_frames,
        xStreamBufferBytesAvailable(EmulatorAudioQueue));
#endif
}

static void EmulatorButtonBegin(EmulatorButton &button, bool pressed)
{
    button.pressed = pressed;
}

static void EmulatorButtonUpdate(EmulatorButton &button,
                                 bool pressed,
                                 uint8_t mac_key)
{
    if (pressed != button.pressed)
    {
        button.pressed = pressed;
        MinivMacAPI_UpdateKey(mac_key, button.pressed ? 1 : 0);
    }
}

static int EmulatorReadBrightness()
{
    int brightness = (int)emulator_encoder().getCount();
    if (brightness < 0)
        brightness = 0;
    if (brightness > kBrightnessMax)
        brightness = kBrightnessMax;
    if (brightness != emulator_encoder().getCount())
        emulator_encoder().setCount(brightness);
    return brightness;
}

static void EmulatorInputsBegin()
{
    EmulatorButtonBegin(EmulatorClockButton, !digitalRead(GPIO_CLOCK));
    EmulatorButtonBegin(EmulatorAlarmButton, !digitalRead(GPIO_ALARM));

    const int brightness = EmulatorReadBrightness();
    EmulatorAppliedBrightness = brightness;
    EmulatorSavedBrightness = brightness;
    EmulatorBrightnessSaveMs = millis();
    EmulatorExitHoldStartMs = 0;
    EmulatorExitRequested = false;
    EmulatorLastEncoder = (int)emulator_encoder().getCount();
    EmulatorLastTouchButton = !digitalRead(GPIO_TOUCH);
    EmulatorFullBrightnessUntilMs = 0;
    analogWrite(TFT_BL_VAR, brightness_to_pwm(brightness));
}

static void EmulatorInputsUpdate()
{
    const uint32_t now = millis();
    const bool clock_pressed = !digitalRead(GPIO_CLOCK);
    const bool alarm_pressed = !digitalRead(GPIO_ALARM);
    const bool touch_pressed = !digitalRead(GPIO_TOUCH);
    const int encoder_position =
        (int)emulator_encoder().getCount();
    const bool hardware_activity =
        (clock_pressed && !EmulatorClockButton.pressed) ||
        (alarm_pressed && !EmulatorAlarmButton.pressed) ||
        (touch_pressed && !EmulatorLastTouchButton) ||
        encoder_position != EmulatorLastEncoder;
    EmulatorLastTouchButton = touch_pressed;
    EmulatorLastEncoder = encoder_position;
    if (hardware_activity)
        EmulatorFullBrightnessUntilMs = now + 10000;

    if (clock_pressed && alarm_pressed)
    {
        if (EmulatorExitHoldStartMs == 0)
            EmulatorExitHoldStartMs = now;
        else if (!EmulatorExitRequested &&
                 (uint32_t)(now - EmulatorExitHoldStartMs) >= kSafeExitHoldMs)
        {
            EmulatorButtonUpdate(EmulatorClockButton, false, kMacKeyEnter);
            EmulatorButtonUpdate(EmulatorAlarmButton, false, kMacKeyEscape);
            EmulatorExitRequested = true;
            MinivMacAPI_RequestSafeExit();
        }
    }
    else
    {
        EmulatorExitHoldStartMs = 0;
        EmulatorButtonUpdate(EmulatorClockButton,
                             clock_pressed,
                             kMacKeyEnter);
        EmulatorButtonUpdate(EmulatorAlarmButton,
                             alarm_pressed,
                             kMacKeyEscape);
    }

    const int brightness = EmulatorReadBrightness();
    const bool force_full_brightness =
        (int32_t)(EmulatorFullBrightnessUntilMs - now) > 0;
    const int applied_brightness =
        force_full_brightness ? kBrightnessMax : brightness;
    if (applied_brightness != EmulatorAppliedBrightness)
    {
        analogWrite(
            TFT_BL_VAR,
            force_full_brightness
                ? 255
                : brightness_to_pwm(brightness));
        EmulatorAppliedBrightness = applied_brightness;
    }
    if (brightness != EmulatorSavedBrightness &&
        (uint32_t)(now - EmulatorBrightnessSaveMs) >= 500)
    {
        emulator_preferences().putUChar("brightness", (uint8_t)brightness);
        EmulatorSavedBrightness = brightness;
        EmulatorBrightnessSaveMs = now;
    }
}

static void DrawEmulatorStartupOverlay()
{
    static constexpr int x = 8;
    static constexpr int y = 174;
    static constexpr int width = 288;
    static constexpr int height = 58;

    my_lcd.fillRect(x, y, width, height, TFT_WHITE);
    my_lcd.drawRect(x, y, width, height, TFT_BLACK);
    my_lcd.setTextColor(TFT_BLACK, TFT_WHITE);
    my_lcd.setTextSize(1);
    my_lcd.drawString(
        tr("Clock: Enter   Alarm: Escape"),
        x + 7, y + 7, 1);
    my_lcd.drawString(
        tr("Hold both 2s: Boot Options"),
        x + 7, y + 23, 1);
    my_lcd.drawString(
        tr("Rotary: Brightness"),
        x + 7, y + 39, 1);
}

static void DrawScreenRegion(const uint8_t *screen_ptr,
                             DirtyRegion region)
{
    static constexpr int border_top = 16;
    static constexpr int border_right = 16;

    int display_width = 0;
    int display_height = 0;
    ArduinoAPI_GetDisplayDimensions(&display_width, &display_height);

    int width = display_width - border_right;
    int height = display_height - border_top;
    if (width > vMacScreenWidth)
        width = vMacScreenWidth;
    if (height > vMacScreenHeight)
        height = vMacScreenHeight;
    if (width <= 0 || height <= 0)
        return;

    if (region.top < 0)
        region.top = 0;
    if (region.left < 0)
        region.left = 0;
    if (region.bottom > height)
        region.bottom = height;
    if (region.right > width)
        region.right = width;
    if (region.bottom <= region.top || region.right <= region.left)
        return;

    region.left &= ~7;
    region.right = (region.right + 7) & ~7;
    if (region.right > width)
        region.right = width;

#ifdef LCD_W
    static uint16_t line_buffer[LCD_W];
    const int line_buffer_width = LCD_W;
#else
    static uint16_t line_buffer[vMacScreenWidth];
    const int line_buffer_width = vMacScreenWidth;
#endif

    const int region_width = region.right - region.left;
    if (region_width <= 0 || region_width > line_buffer_width)
        return;

    const uint16_t color_on = 0x0000;
    const uint16_t color_off = 0xFFFF;
    const int pitch_bytes = (vMacScreenWidth + 7) / 8;
#ifdef MINIVMAC_PROFILE
    const uint64_t profile_start_us = (uint64_t)esp_timer_get_time();
#endif

    my_lcd.startWrite();
    my_lcd.setAddrWindow(region.left,
                         border_top + region.top,
                         region_width,
                         region.bottom - region.top);

    for (int y = region.top; y < region.bottom; ++y)
    {
        const uint8_t *src =
            screen_ptr + y * pitch_bytes + region.left / 8;
        uint16_t *dst = line_buffer;
        int remaining = region_width;
        while (remaining >= 8)
        {
            const uint8_t byte = *src++;
            dst[0] = (byte & 0x80) ? color_on : color_off;
            dst[1] = (byte & 0x40) ? color_on : color_off;
            dst[2] = (byte & 0x20) ? color_on : color_off;
            dst[3] = (byte & 0x10) ? color_on : color_off;
            dst[4] = (byte & 0x08) ? color_on : color_off;
            dst[5] = (byte & 0x04) ? color_on : color_off;
            dst[6] = (byte & 0x02) ? color_on : color_off;
            dst[7] = (byte & 0x01) ? color_on : color_off;
            dst += 8;
            remaining -= 8;
        }
        if (remaining > 0)
        {
            const uint8_t byte = *src;
            for (int bit = 0; bit < remaining; ++bit)
                *dst++ = (byte & (uint8_t)(0x80 >> bit))
                             ? color_on
                             : color_off;
        }

        my_lcd.pushColors(line_buffer, (uint32_t)region_width, true);
    }

    my_lcd.endWrite();
#ifdef MINIVMAC_PROFILE
    EmulatorProfileAddRender(
        (uint64_t)esp_timer_get_time() - profile_start_us,
        (uint32_t)region_width *
            (uint32_t)(region.bottom - region.top));
#endif
}

void RenderTask(void *Param)
{
    (void)Param;

    while (true)
    {
        TickType_t wait_ticks = portMAX_DELAY;
        const int32_t overlay_remaining =
            (int32_t)(EmulatorOverlayUntilMs - millis());
        if (overlay_remaining > 0)
        {
            wait_ticks = pdMS_TO_TICKS((uint32_t)overlay_remaining);
            if (wait_ticks == 0)
                wait_ticks = 1;
        }
        else if (EmulatorOverlayDrawn)
        {
            wait_ticks = 0;
        }

        xEventGroupWaitBits(RenderTaskEventHandle, DrawScreenEvent,
                            pdTRUE, pdTRUE, wait_ticks);
        if (RenderTaskShouldStop)
            break;

        DirtyRegion region = {};
        const uint8_t *screen_ptr = NULL;
        portENTER_CRITICAL(&Crit);
        region = PendingRenderRegion;
        PendingRenderRegion.valid = false;
        screen_ptr = EmScreenPtr;
        portEXIT_CRITICAL(&Crit);

        const bool overlay_visible =
            (int32_t)(EmulatorOverlayUntilMs - millis()) > 0;
        if (!overlay_visible && EmulatorOverlayDrawn)
        {
            MergeDirtyRegion(region, 174 - 16, 8, 174 - 16 + 58, 8 + 288);
            EmulatorOverlayDrawn = false;
        }

        if (!screen_ptr || !region.valid)
            continue;

        xSemaphoreTake(DisplayLock, portMAX_DELAY);
        DrawScreenRegion(screen_ptr, region);
        if ((int32_t)(EmulatorOverlayUntilMs - millis()) > 0)
        {
            DrawEmulatorStartupOverlay();
            EmulatorOverlayDrawn = true;
        }
        xSemaphoreGive(DisplayLock);
    }

    RenderTaskHandle = NULL;
    vTaskDelete(NULL);
}

void minivmac(void)
{
#ifdef MINIVMAC_PROFILE
    EmulatorProfileReset();
#endif
    my_lcd.fillScreen(TFT_BLACK);

    RenderTaskEventHandle = xEventGroupCreate();
    DisplayLock = xSemaphoreCreateMutex();
    FileSystemLock = xSemaphoreCreateMutex();
    portENTER_CRITICAL(&Crit);
    EmScreenPtr = NULL;
    ChangedScreenRegion.valid = false;
    PendingRenderRegion.valid = false;
    portEXIT_CRITICAL(&Crit);
    RenderTaskShouldStop = false;
    EmulatorOverlayUntilMs = millis() + 4000;
    EmulatorOverlayDrawn = false;

    xTaskCreatePinnedToCore(RenderTask, "RenderTask", 4096, NULL, 0, &RenderTaskHandle, 0);

    Mouse.Init();
    EmulatorInputsBegin();

    minivmac_main(0, NULL);

    RenderTaskShouldStop = true;
    if (RenderTaskEventHandle)
        xEventGroupSetBits(RenderTaskEventHandle, DrawScreenEvent);
    for (int i = 0; RenderTaskHandle && i < 50; ++i)
        vTaskDelay(pdMS_TO_TICKS(10));
    if (RenderTaskHandle)
    {
        vTaskDelete(RenderTaskHandle);
        RenderTaskHandle = NULL;
    }

    portENTER_CRITICAL(&Crit);
    EmScreenPtr = NULL;
    ChangedScreenRegion.valid = false;
    PendingRenderRegion.valid = false;
    portEXIT_CRITICAL(&Crit);
    if (DisplayLock)
    {
        vSemaphoreDelete(DisplayLock);
        DisplayLock = NULL;
    }
    if (FileSystemLock)
    {
        vSemaphoreDelete(FileSystemLock);
        FileSystemLock = NULL;
    }
    if (RenderTaskEventHandle)
    {
        vEventGroupDelete(RenderTaskEventHandle);
        RenderTaskEventHandle = NULL;
    }
}

void ArduinoAPI_GetDisplayDimensions(int *OutWidthPtr, int *OutHeightPtr)
{
    if (OutWidthPtr)
    {
        *OutWidthPtr = (int)my_lcd.width();
    }
    if (OutHeightPtr)
    {
        *OutHeightPtr = (int)my_lcd.height();
    }
}

void ArduinoAPI_SetAddressWindow(int x0, int y0, int x1, int y1)
{
    my_lcd.setAddrWindow(x0, y0, (x1 - x0), (y1 - y0));
}

void ArduinoAPI_WritePixels(const uint16_t *Pixels, size_t Count)
{
    my_lcd.startWrite();
    my_lcd.pushColors((uint16_t *)Pixels, (uint32_t)Count, true);
    my_lcd.endWrite();
}

void ArduinoAPI_GetMouseDelta(int *OutXDeltaPtr, int *OutYDeltaPtr)
{
    float x = 0.0f;
    float y = 0.0f;

    Mouse.Read(x, y);

    *OutXDeltaPtr = (int)x;
    *OutYDeltaPtr = (int)y;
}

void ArduinoAPI_GiveEmulatedMouseToArduino(int *EmMouseX, int *EmMouseY)
{
    vMacMouseX = *EmMouseX;
    vMacMouseY = *EmMouseY;
}

int ArduinoAPI_GetMouseButton(void)
{
    return (int)Mouse.ReadButton();
}

uint64_t ArduinoAPI_GetTimeMS(void)
{
    return (uint64_t)millis();
}

#ifdef MINIVMAC_PROFILE
uint64_t ArduinoAPI_GetTimeUS(void)
{
    return (uint64_t)esp_timer_get_time();
}

void ArduinoAPI_ProfileEmulationWork(uint64_t DurationUS, int Lag)
{
    portENTER_CRITICAL(&EmulatorProfileCrit);
    EmulatorProfile.EmulationWorkUS += DurationUS;
    ++EmulatorProfile.EmulationLoops;
    if (Lag > EmulatorProfile.MaxLag)
        EmulatorProfile.MaxLag = Lag;
    portEXIT_CRITICAL(&EmulatorProfileCrit);
}

void ArduinoAPI_ProfileEmulationWait(uint64_t DurationUS)
{
    portENTER_CRITICAL(&EmulatorProfileCrit);
    EmulatorProfile.EmulationWaitUS += DurationUS;
    portEXIT_CRITICAL(&EmulatorProfileCrit);
}
#endif

void ArduinoAPI_Yield(void)
{
    yield();
}

void ArduinoAPI_Delay(uint32_t MSToDelay)
{
    delay(MSToDelay);
}

ArduinoFile ArduinoAPI_open(const char *Path, const char *Mode)
{
    ArduinoFile Handle = NULL;
    const char *NormalizedPath = Path;
    char LittleFSPath[256];

    if (!Path || !Mode)
    {
        return NULL;
    }

    if (Path && Path[0] != '/')
    {
        snprintf(LittleFSPath, sizeof(LittleFSPath), "/%s", Path);
        NormalizedPath = LittleFSPath;
    }

    xSemaphoreTake(FileSystemLock, portMAX_DELAY);
    fs::File File = LittleFS.open(NormalizedPath, Mode);
    if (File)
    {
        ArduinoFileState *State = new ArduinoFileState;
        if (State)
        {
            State->File = File;
            State->Position = (uint32_t)File.position();
            State->CacheStart = 0;
            State->CacheLength = 0;
            State->CacheValid = false;
            Handle = State;
        }
        else
        {
            File.close();
        }
    }
    xSemaphoreGive(FileSystemLock);

    return Handle;
}

void ArduinoAPI_close(ArduinoFile Handle)
{
    if (Handle)
    {
        ArduinoFileState *State = (ArduinoFileState *)Handle;
        xSemaphoreTake(FileSystemLock, portMAX_DELAY);
        State->File.close();
        xSemaphoreGive(FileSystemLock);
        delete State;
    }
}

size_t ArduinoAPI_read(void *Buffer, size_t Size, size_t Nmemb, ArduinoFile Handle)
{
    if (!Handle || !Buffer || Size == 0 || Nmemb == 0 ||
        Nmemb > (SIZE_MAX / Size))
    {
        return 0;
    }

    ArduinoFileState *State = (ArduinoFileState *)Handle;
    uint8_t *Destination = (uint8_t *)Buffer;
    const size_t BytesRequested = Size * Nmemb;
    size_t BytesRead = 0;
#ifdef MINIVMAC_PROFILE
    uint64_t ProfileCacheHitBytes = 0;
    uint64_t ProfileFilesystemReadBytes = 0;
    uint32_t ProfileCacheMisses = 0;
    bool ProfileCacheJustFilled = false;
#endif

    xSemaphoreTake(FileSystemLock, portMAX_DELAY);
    while (BytesRead < BytesRequested)
    {
        if (State->CacheValid &&
            State->Position >= State->CacheStart &&
            State->Position < State->CacheStart + State->CacheLength)
        {
            const size_t CacheOffset = State->Position - State->CacheStart;
            const size_t CacheAvailable = State->CacheLength - CacheOffset;
            const size_t Remaining = BytesRequested - BytesRead;
            const size_t CopyLength =
                (CacheAvailable < Remaining) ? CacheAvailable : Remaining;

            memcpy(Destination + BytesRead, State->Cache + CacheOffset, CopyLength);
#ifdef MINIVMAC_PROFILE
            if (!ProfileCacheJustFilled)
                ProfileCacheHitBytes += CopyLength;
            ProfileCacheJustFilled = false;
#endif
            State->Position += CopyLength;
            BytesRead += CopyLength;
            continue;
        }

        const size_t Remaining = BytesRequested - BytesRead;
        if (Remaining >= kDiskCacheSize)
        {
            State->CacheValid = false;
            if (!State->File.seek(State->Position, fs::SeekSet))
                break;

            const size_t DirectRead =
                State->File.read(Destination + BytesRead, Remaining);
#ifdef MINIVMAC_PROFILE
            ProfileFilesystemReadBytes += DirectRead;
            ProfileCacheJustFilled = false;
#endif
            State->Position += DirectRead;
            BytesRead += DirectRead;
            if (DirectRead == 0)
                break;
            continue;
        }

        const uint32_t CacheStart =
            State->Position - (State->Position % kDiskCacheSize);
        if (!State->File.seek(CacheStart, fs::SeekSet))
            break;

        State->CacheStart = CacheStart;
        State->CacheLength = State->File.read(State->Cache, kDiskCacheSize);
#ifdef MINIVMAC_PROFILE
        ProfileFilesystemReadBytes += State->CacheLength;
        ++ProfileCacheMisses;
        ProfileCacheJustFilled = true;
#endif
        State->CacheValid =
            (State->Position - CacheStart) < State->CacheLength;
        if (!State->CacheValid)
            break;
    }
    xSemaphoreGive(FileSystemLock);
#ifdef MINIVMAC_PROFILE
    EmulatorProfileAddDiskRead(
        ProfileCacheHitBytes,
        ProfileFilesystemReadBytes,
        ProfileCacheMisses);
#endif

    return BytesRead / Size;
}

size_t ArduinoAPI_write(const void *Buffer, size_t Size, size_t Nmemb, ArduinoFile Handle)
{
    size_t ElementsWritten = 0;
#ifdef MINIVMAC_PROFILE
    size_t ProfileBytesWritten = 0;
#endif

    if (Handle && Buffer && Size > 0 && Nmemb > 0 &&
        Nmemb <= (SIZE_MAX / Size))
    {
        ArduinoFileState *State = (ArduinoFileState *)Handle;
        const size_t BytesRequested = Size * Nmemb;
        xSemaphoreTake(FileSystemLock, portMAX_DELAY);
        State->CacheValid = false;
        if (State->File.seek(State->Position, fs::SeekSet))
        {
            const size_t BytesWritten =
                State->File.write((const uint8_t *)Buffer, BytesRequested);
#ifdef MINIVMAC_PROFILE
            ProfileBytesWritten = BytesWritten;
#endif
            State->Position += BytesWritten;
            ElementsWritten = BytesWritten / Size;
        }
        xSemaphoreGive(FileSystemLock);
    }
#ifdef MINIVMAC_PROFILE
    if (ProfileBytesWritten > 0)
        EmulatorProfileAddDiskWrite(ProfileBytesWritten);
#endif

    return ElementsWritten;
}

long ArduinoAPI_tell(ArduinoFile Handle)
{
    long Offset = 0;

    if (Handle)
    {
        ArduinoFileState *State = (ArduinoFileState *)Handle;
        xSemaphoreTake(FileSystemLock, portMAX_DELAY);
        Offset = (long)State->Position;
        xSemaphoreGive(FileSystemLock);
    }

    return Offset;
}

long ArduinoAPI_seek(ArduinoFile Handle, long Offset, int Whence)
{
    long Result = -1;

    if (Handle)
    {
        ArduinoFileState *State = (ArduinoFileState *)Handle;
        xSemaphoreTake(FileSystemLock, portMAX_DELAY);
        int64_t Base = 0;
        if (Whence == Arduino_Seek_Cur)
        {
            Base = (int64_t)State->Position;
        }
        else if (Whence == Arduino_Seek_End)
        {
            Base = (int64_t)State->File.size();
        }

        int64_t Target = Base + (int64_t)Offset;
        if (Target >= 0 && Target <= UINT32_MAX)
        {
            State->Position = (uint32_t)Target;
            Result = 0;
        }
        xSemaphoreGive(FileSystemLock);
    }

    return Result;
}

int ArduinoAPI_eof(ArduinoFile Handle)
{
    int IsEOF = 0;

    if (Handle)
    {
        ArduinoFileState *State = (ArduinoFileState *)Handle;
        xSemaphoreTake(FileSystemLock, portMAX_DELAY);
        IsEOF = (State->Position >= State->File.size()) ? 1 : 0;
        xSemaphoreGive(FileSystemLock);
    }

    return IsEOF;
}

void *ArduinoAPI_malloc(size_t Size)
{
    return heap_caps_malloc(Size, (Size >= 262144) ? MALLOC_CAP_SPIRAM : MALLOC_CAP_DEFAULT);
}

void *ArduinoAPI_calloc(size_t Nmemb, size_t Size)
{
    return heap_caps_calloc(Nmemb, Size, ((Size * Nmemb) >= 262144) ? MALLOC_CAP_SPIRAM : MALLOC_CAP_DEFAULT);
}

void *ArduinoAPI_calloc_internal(size_t Nmemb, size_t Size)
{
    return heap_caps_calloc(
        Nmemb,
        Size,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

void ArduinoAPI_free(void *Memory)
{
    heap_caps_free(Memory);
}

void ArduinoAPI_CheckForEvents(void)
{
    EmulatorInputsUpdate();
    Mouse.Update( );
#ifdef MINIVMAC_PROFILE
    EmulatorProfileMaybeReport();
#endif
}

void ArduinoAPI_ScreenChanged(int Top, int Left, int Bottom, int Right)
{
    if (Top < 0)
        Top = 0;
    if (Left < 0)
        Left = 0;
    if (Bottom > vMacScreenHeight)
        Bottom = vMacScreenHeight;
    if (Right > vMacScreenWidth)
        Right = vMacScreenWidth;

    portENTER_CRITICAL(&Crit);
    MergeDirtyRegion(ChangedScreenRegion, Top, Left, Bottom, Right);
    portEXIT_CRITICAL(&Crit);
}

void ArduinoAPI_DrawScreen(const uint8_t *Screen)
{
    bool needs_render = false;

    portENTER_CRITICAL(&Crit);
    EmScreenPtr = Screen;
    if (ChangedScreenRegion.valid)
    {
        MergeDirtyRegion(PendingRenderRegion,
                         ChangedScreenRegion.top,
                         ChangedScreenRegion.left,
                         ChangedScreenRegion.bottom,
                         ChangedScreenRegion.right);
        ChangedScreenRegion.valid = false;
        needs_render = true;
    }
    portEXIT_CRITICAL(&Crit);

    if (needs_render && RenderTaskEventHandle)
        xEventGroupSetBits(RenderTaskEventHandle, DrawScreenEvent);
}
