#include <string.h>

#include <Arduino.h>
#include <Wire.h>

#include <FS.h>
#include <LittleFS.h>

#include <ESP32Encoder.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "ArduinoAPI.h"
#include "brightness.h"

#include "SYSDEPNS.h"
#include "CNFGGLOB.h"
#include "CNFGRAPI.h"
#include "MYOSGLUE.h"

#include "mouse.h"

extern TFT_eSPI my_lcd;
extern ESP32Encoder encoder;
extern Preferences preferences;

int vMacMouseX = 0;
int vMacMouseY = 0;

bool ScreenNeedsUpdate = false;

portMUX_TYPE Crit = portMUX_INITIALIZER_UNLOCKED;

#define DrawScreenEvent 0x01

EventGroupHandle_t RenderTaskEventHandle = NULL;
SemaphoreHandle_t RenderTaskLock = NULL;
SemaphoreHandle_t SPIBusLock = NULL;
TaskHandle_t RenderTaskHandle = NULL;

volatile const uint8_t *EmScreenPtr = NULL;
static volatile bool RenderTaskShouldStop = false;
static volatile uint32_t EmulatorOverlayUntilMs = 0;

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

void RenderTask(void *Param)
{
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

        xEventGroupWaitBits(RenderTaskEventHandle, DrawScreenEvent,
                            pdTRUE, pdTRUE, wait_ticks);
        if (RenderTaskShouldStop)
            break;

        if (xSemaphoreTake(SPIBusLock, pdMS_TO_TICKS(5)) == pdTRUE)
        {
            xSemaphoreTake(RenderTaskLock, portMAX_DELAY);
            const uint8_t *screen_ptr = (const uint8_t *)EmScreenPtr;
            if (screen_ptr)
            {
                int display_width = 0;
                int display_height = 0;
                ArduinoAPI_GetDisplayDimensions(&display_width, &display_height);

                const int vmac_width = vMacScreenWidth;
                const int vmac_height = vMacScreenHeight;

                if (display_width > 0 && display_height > 0 && vmac_width > 0 && vmac_height > 0)
                {
                    const int border_top = 16;
                    const int border_right = 16;
                    const int src_x = 0;
                    const int src_y = 0;

                    int out_width = display_width;
                    int out_height = display_height;

#ifdef LCD_W
                    static uint16_t line_buffer[LCD_W];
                    const int line_buffer_width = LCD_W;
#else
                    static uint16_t line_buffer[vMacScreenWidth];
                    const int line_buffer_width = vMacScreenWidth;
#endif

                    if (out_width > line_buffer_width)
                    {
                        out_width = line_buffer_width;
                    }
                    if (out_height < 0)
                    {
                        out_height = 0;
                    }

                    int width = out_width - border_right;
                    int height = out_height - border_top;

                    if (width < 0)
                    {
                        width = 0;
                    }
                    if (height < 0)
                    {
                        height = 0;
                    }

                    if (width > (vmac_width - src_x))
                    {
                        width = vmac_width - src_x;
                    }
                    if (height > (vmac_height - src_y))
                    {
                        height = vmac_height - src_y;
                    }

                    if (out_width > 0 && out_height > 0)
                    {
                        const uint16_t color_on = 0x0000;
                        const uint16_t color_off = 0xFFFF;
                        const int pitch_bytes = (vmac_width + 7) / 8;
                        const uint16_t color_border = 0x0000;

                        my_lcd.startWrite();
                        my_lcd.setAddrWindow(0, 0, out_width, out_height);

                        for (int y = 0; y < out_height; ++y)
                        {
                            uint16_t *dst = line_buffer;

                            if (y < border_top || y >= (border_top + height))
                            {
                                for (int i = 0; i < out_width; ++i)
                                {
                                    *dst++ = color_border;
                                }
                                my_lcd.pushColors(line_buffer, (uint32_t)out_width, true);
                                continue;
                            }

                            const int src_row = y - border_top;
                            const uint8_t *row_ptr = screen_ptr + ((src_y + src_row) * pitch_bytes) + (src_x / 8);
                            int remaining = width;

                            while (remaining >= 8)
                            {
                                const uint8_t byte = *row_ptr++;
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
                                const uint8_t byte = *row_ptr++;
                                for (int bit = 0; bit < remaining; ++bit)
                                {
                                    const uint8_t mask = (uint8_t)(0x80 >> bit);
                                    *dst++ = (byte & mask) ? color_on : color_off;
                                }
                            }

                            for (int i = width; i < out_width; ++i)
                            {
                                *dst++ = color_border;
                            }

                            my_lcd.pushColors(line_buffer, (uint32_t)out_width, true);
                        }

                        my_lcd.endWrite();

                        if ((int32_t)(EmulatorOverlayUntilMs - millis()) > 0)
                            DrawEmulatorStartupOverlay();
                    }
                }
            }
            xSemaphoreGive(RenderTaskLock);
            xSemaphoreGive(SPIBusLock);
        }
    }

    RenderTaskHandle = NULL;
    vTaskDelete(NULL);
}

void minivmac(void)
{
    RenderTaskEventHandle = xEventGroupCreate();
    RenderTaskLock = xSemaphoreCreateMutex();
    SPIBusLock = xSemaphoreCreateMutex();
    RenderTaskShouldStop = false;
    EmulatorOverlayUntilMs = millis() + 4000;

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

    EmScreenPtr = NULL;
    ScreenNeedsUpdate = false;
    if (RenderTaskLock)
    {
        vSemaphoreDelete(RenderTaskLock);
        RenderTaskLock = NULL;
    }
    if (SPIBusLock)
    {
        vSemaphoreDelete(SPIBusLock);
        SPIBusLock = NULL;
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

    xSemaphoreTake(SPIBusLock, portMAX_DELAY);
    fs::File File = LittleFS.open(NormalizedPath, Mode);
    if (File)
    {
        Handle = new fs::File(File);
    }
    xSemaphoreGive(SPIBusLock);

    return Handle;
}

void ArduinoAPI_close(ArduinoFile Handle)
{
    if (Handle)
    {
        fs::File *File = (fs::File *)Handle;
        xSemaphoreTake(SPIBusLock, portMAX_DELAY);
        File->close();
        xSemaphoreGive(SPIBusLock);
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
        xSemaphoreTake(SPIBusLock, portMAX_DELAY);
        size_t BytesRead = File->read((uint8_t *)Buffer, BytesRequested);
        ElementsRead = BytesRead / Size;
        xSemaphoreGive(SPIBusLock);
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
        xSemaphoreTake(SPIBusLock, portMAX_DELAY);
        size_t BytesWritten = File->write((const uint8_t *)Buffer, BytesRequested);
        ElementsWritten = BytesWritten / Size;
        xSemaphoreGive(SPIBusLock);
    }

    return ElementsWritten;
}

long ArduinoAPI_tell(ArduinoFile Handle)
{
    long Offset = 0;

    if (Handle)
    {
        fs::File *File = (fs::File *)Handle;
        xSemaphoreTake(SPIBusLock, portMAX_DELAY);
        Offset = (long)File->position();
        xSemaphoreGive(SPIBusLock);
    }

    return Offset;
}

long ArduinoAPI_seek(ArduinoFile Handle, long Offset, int Whence)
{
    long Result = -1;

    if (Handle)
    {
        fs::File *File = (fs::File *)Handle;
        xSemaphoreTake(SPIBusLock, portMAX_DELAY);
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
        xSemaphoreGive(SPIBusLock);
    }

    return Result;
}

int ArduinoAPI_eof(ArduinoFile Handle)
{
    int IsEOF = 0;

    if (Handle)
    {
        fs::File *File = (fs::File *)Handle;
        xSemaphoreTake(SPIBusLock, portMAX_DELAY);
        IsEOF = (File->position() >= File->size()) ? 1 : 0;
        xSemaphoreGive(SPIBusLock);
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
    ScreenNeedsUpdate = true;
}

void ArduinoAPI_DrawScreen(const uint8_t *Screen)
{
    if (ScreenNeedsUpdate)
    {
        ScreenNeedsUpdate = false;
        EmScreenPtr = Screen;

        if (xSemaphoreTake(RenderTaskLock, 0) == pdTRUE)
        {
            xSemaphoreGive(RenderTaskLock);

            xEventGroupSetBits(RenderTaskEventHandle, DrawScreenEvent);
        }
    }
}
