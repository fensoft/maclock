#include <string.h>

#include <Arduino.h>
#include <Wire.h>

#include <FS.h>
#include <LittleFS.h>

#include <ESP32Encoder.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "AudioOutputI2S.h"
#include "ArduinoAPI.h"
#include "audio_output.h"
#include "brightness.h"
#include "es8311.h"

#include "SYSDEPNS.h"
#include "CNFGGLOB.h"
#include "CNFGRAPI.h"
#include "MYOSGLUE.h"

#include "mouse.h"

extern TFT_eSPI my_lcd;
extern ESP32Encoder encoder;
extern Preferences preferences;
extern AudioOutputI2S *audio_out;
extern es8311_handle_t es8311_handle;
extern void setup_codec();

int vMacMouseX = 0;
int vMacMouseY = 0;

portMUX_TYPE Crit = portMUX_INITIALIZER_UNLOCKED;

#define DrawScreenEvent 0x01

EventGroupHandle_t RenderTaskEventHandle = NULL;
SemaphoreHandle_t DisplayLock = NULL;
SemaphoreHandle_t FileSystemLock = NULL;
TaskHandle_t RenderTaskHandle = NULL;

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
static bool EmulatorSoundInitialized = false;
static bool EmulatorSoundStarted = false;
static uint32_t EmulatorSoundSampleRate = 0;

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
    setup_codec();
    if (!audio_out || !es8311_handle)
        return 0;

    audio_out->stop();
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
    return EmulatorSoundStarted ? 1 : 0;
}

void ArduinoAPI_Sound_Stop()
{
    if (!EmulatorSoundStarted)
        return;
    audio_out->stop();
    EmulatorSoundStarted = false;
}

void ArduinoAPI_Sound_UnInit()
{
    static constexpr uint32_t kClockAudioRate = 44100;
    static constexpr uint32_t kClockAudioMclkMultiplier = 256;

    ArduinoAPI_Sound_Stop();
    if (es8311_handle)
    {
        es8311_sample_frequency_config(
            es8311_handle,
            kClockAudioRate * kClockAudioMclkMultiplier,
            kClockAudioRate);
    }
    if (audio_out)
    {
        audio_out->SetBuffers(8, 8192);
        audio_out->SetRate((int)kClockAudioRate);
        audio_out->SetChannels(2);
    }
    EmulatorSoundInitialized = false;
    EmulatorSoundSampleRate = 0;
}

void ArduinoAPI_Sound_Write(const uint8_t *samples, size_t count)
{
    if (!EmulatorSoundStarted || !samples)
        return;

    static constexpr size_t kAudioBlockFrames = 512;
    static int16_t stereo_frames[kAudioBlockFrames * 2];

    size_t offset = 0;
    while (offset < count && EmulatorSoundStarted)
    {
        const size_t block_frames =
            min(count - offset, kAudioBlockFrames);
        for (size_t i = 0; i < block_frames; ++i)
        {
            const int16_t sample =
                (int16_t)(((int32_t)samples[offset + i] - 128) * 256);
            stereo_frames[i * 2] = sample;
            stereo_frames[i * 2 + 1] = sample;
        }

        size_t frames_written = 0;
        while (frames_written < block_frames && EmulatorSoundStarted)
        {
            const size_t written = audio_write_stereo_frames(
                &stereo_frames[frames_written * 2],
                block_frames - frames_written);
            if (written == 0)
                vTaskDelay(1);
            else
                frames_written += written;
        }
        offset += block_frames;
    }
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
    int brightness = (int)encoder.getCount();
    if (brightness < 0)
        brightness = 0;
    if (brightness > kBrightnessMax)
        brightness = kBrightnessMax;
    if (brightness != encoder.getCount())
        encoder.setCount(brightness);
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
    analogWrite(TFT_BL_VAR, brightness_to_pwm(brightness));
}

static void EmulatorInputsUpdate()
{
    const uint32_t now = millis();
    const bool clock_pressed = !digitalRead(GPIO_CLOCK);
    const bool alarm_pressed = !digitalRead(GPIO_ALARM);

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
    if (brightness != EmulatorAppliedBrightness)
    {
        analogWrite(TFT_BL_VAR, brightness_to_pwm(brightness));
        EmulatorAppliedBrightness = brightness;
    }
    if (brightness != EmulatorSavedBrightness &&
        (uint32_t)(now - EmulatorBrightnessSaveMs) >= 500)
    {
        preferences.putUChar("brightness", (uint8_t)brightness);
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
    my_lcd.drawString("Clock: Enter   Alarm: Escape", x + 7, y + 7, 1);
    my_lcd.drawString("Hold both 2s: Boot Options", x + 7, y + 23, 1);
    my_lcd.drawString("Rotary: Brightness", x + 7, y + 39, 1);
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

    // Mini vMac stores eight monochrome pixels per byte. Expanding an
    // aligned region avoids per-pixel source addressing while adding at
    // most seven harmless pixels on either side.
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
        Handle = new fs::File(File);
    }
    xSemaphoreGive(FileSystemLock);

    return Handle;
}

void ArduinoAPI_close(ArduinoFile Handle)
{
    if (Handle)
    {
        fs::File *File = (fs::File *)Handle;
        xSemaphoreTake(FileSystemLock, portMAX_DELAY);
        File->close();
        xSemaphoreGive(FileSystemLock);
        delete File;
    }
}

size_t ArduinoAPI_read(void *Buffer, size_t Size, size_t Nmemb, ArduinoFile Handle)
{
    size_t ElementsRead = 0;

    if (Handle && Size > 0 && Nmemb > 0)
    {
        fs::File *File = (fs::File *)Handle;
        size_t BytesRequested = Size * Nmemb;
        xSemaphoreTake(FileSystemLock, portMAX_DELAY);
        size_t BytesRead = File->read((uint8_t *)Buffer, BytesRequested);
        ElementsRead = BytesRead / Size;
        xSemaphoreGive(FileSystemLock);
    }

    return ElementsRead;
}

size_t ArduinoAPI_write(const void *Buffer, size_t Size, size_t Nmemb, ArduinoFile Handle)
{
    size_t ElementsWritten = 0;

    if (Handle && Size > 0 && Nmemb > 0)
    {
        fs::File *File = (fs::File *)Handle;
        size_t BytesRequested = Size * Nmemb;
        xSemaphoreTake(FileSystemLock, portMAX_DELAY);
        size_t BytesWritten = File->write((const uint8_t *)Buffer, BytesRequested);
        ElementsWritten = BytesWritten / Size;
        xSemaphoreGive(FileSystemLock);
    }

    return ElementsWritten;
}

long ArduinoAPI_tell(ArduinoFile Handle)
{
    long Offset = 0;

    if (Handle)
    {
        fs::File *File = (fs::File *)Handle;
        xSemaphoreTake(FileSystemLock, portMAX_DELAY);
        Offset = (long)File->position();
        xSemaphoreGive(FileSystemLock);
    }

    return Offset;
}

long ArduinoAPI_seek(ArduinoFile Handle, long Offset, int Whence)
{
    long Result = -1;

    if (Handle)
    {
        fs::File *File = (fs::File *)Handle;
        xSemaphoreTake(FileSystemLock, portMAX_DELAY);
        int64_t Base = 0;
        if (Whence == Arduino_Seek_Cur)
        {
            Base = (int64_t)File->position();
        }
        else if (Whence == Arduino_Seek_End)
        {
            Base = (int64_t)File->size();
        }

        int64_t Target = Base + (int64_t)Offset;
        if (Target >= 0 && Target <= UINT32_MAX)
        {
            Result = File->seek((uint32_t)Target, fs::SeekSet) ? 0 : -1;
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
        fs::File *File = (fs::File *)Handle;
        xSemaphoreTake(FileSystemLock, portMAX_DELAY);
        IsEOF = (File->position() >= File->size()) ? 1 : 0;
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
