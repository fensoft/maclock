#include <lvgl.h>
#include "src/draw/lv_image_decoder_private.h"
#include "AudioFileSourceLittleFS.h"
#include "AudioGeneratorOpus.h"
#include "AudioOutputI2S.h"
#include "AudioGeneratorMP3.h"
#include "es8311.h"
#include <RTClib.h>
#include <esp_heap_caps.h>
#include <ESP32Encoder.h>
#include <TFT_eSPI.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "driver/touch_pad.h"
#include "TouchSensor.h"
#include <Wire.h>
#include <LittleFS.h>
#include "datetime_ui.h"
#include "touch.h"
#include "Adafruit_BMP5xx.h"

LV_FONT_DECLARE(lv_font_chicago_8);
LV_FONT_DECLARE(lv_font_chicago_32);
LV_FONT_DECLARE(lv_font_chicago_48);

TouchSensor touch(GPIO_TOUCH);
ESP32Encoder encoder;
AudioFileSourceLittleFS *file;
extern AudioOutputI2S *audio_out;
extern es8311_handle_t es8311_handle;
AudioGeneratorMP3 *mp3 = NULL;
extern TFT_eSPI my_lcd;
extern es8311_handle_t es8311_handle;
RTC_DS1307 rtc;
Adafruit_BMP5xx bmp;

struct InputState
{
    bool floppy;
    bool alarm;
    bool clock;
    bool touch;
};

static constexpr size_t k_plugin_max = 5;

struct UiImages
{
    lv_obj_t *background;
    lv_obj_t *corners;
    lv_obj_t *disk_missing_1;
    lv_obj_t *disk_missing_2;
    lv_obj_t *boot;
    lv_obj_t *menu;
    lv_obj_t *menu_right;
    lv_obj_t *icon;
    lv_obj_t *clock;
    lv_obj_t *clock_label;
    lv_obj_t *time;
    lv_obj_t *date;
    lv_obj_t *temp;
    lv_obj_t *gauge_icon;
    lv_obj_t *gauge_line;
    lv_obj_t *gauge_box;
    lv_obj_t *white_bar;
    lv_obj_t *black_line;
    lv_obj_t *plugin_icons[k_plugin_max];

    lv_draw_buf_t *background_buf;
    lv_draw_buf_t *corners_buf;
    lv_draw_buf_t *disk_missing_1_buf;
    lv_draw_buf_t *disk_missing_2_buf;
    lv_draw_buf_t *boot_buf;
    lv_draw_buf_t *menu_buf;
    lv_draw_buf_t *menu_right_buf;
    lv_draw_buf_t *icon_buf;
    lv_draw_buf_t *clock_buf;
    lv_draw_buf_t *plugin_buf;
    lv_draw_buf_t *plugin_missing_buf;
};

struct CalibUi
{
    lv_obj_t *label;
    lv_obj_t *cross;
};

enum UiState
{
    UI_STATE_EMPTY_SCREEN = 1,
    UI_STATE_WAIT_STARTUP_SOUND = 2,
    UI_STATE_WAIT_FLOPPY_1 = 3,
    UI_STATE_WAIT_FLOPPY_2 = 4,
    UI_STATE_FLOPPY_INSERTED = 5,
    UI_STATE_BOOT_PLUGINS = 6,
    UI_STATE_WAIT_FLOPPY_SOUND = 7,
    UI_STATE_NORMAL = 8,
    UI_STATE_SET_DATETIME = 9,
    UI_STATE_CALIBRATION = 10
};

static InputState g_input_state = {};
static portMUX_TYPE g_input_state_mux = portMUX_INITIALIZER_UNLOCKED;
static UiImages g_ui = {};
static CalibUi g_calib_ui = {};
static bool g_mp3_finished = false;
static portMUX_TYPE g_mp3_mux = portMUX_INITIALIZER_UNLOCKED;
static int g_requested_state = 0;
static lv_obj_t *g_cursor = nullptr;
static lv_timer_t *g_cursor_timer = nullptr;
static constexpr uint8_t k_nvram_addr_encoder = 0;

void setup_codec();
void setup_lvgl_display();
void setup_lvgl_input();
void lvgl_fs_init_littlefs();
void minivmac();

void request_state(int state)
{
    g_requested_state = state;
}

static void cursor_hide_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (g_cursor)
        lv_obj_add_flag(g_cursor, LV_OBJ_FLAG_HIDDEN);
}

static void cursor_show_at(const lv_point_t &p)
{
    if (!g_cursor)
        return;
    const int16_t hot_x = 0;
    const int16_t hot_y = 0;
    lv_obj_clear_flag(g_cursor, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(g_cursor, p.x - hot_x, p.y - hot_y);
    lv_obj_move_foreground(g_cursor);
    if (!g_cursor_timer)
        g_cursor_timer = lv_timer_create(cursor_hide_timer_cb, 2000, nullptr);
    else
        lv_timer_reset(g_cursor_timer);
}

static void screen_touch_event(lv_event_t *e)
{
    (void)e;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev)
        return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    cursor_show_at(p);
}

static void calib_set_cross_pos(const lv_point_t &pt)
{
    if (!g_calib_ui.cross)
        return;
    lv_obj_t *scr = lv_screen_active();
    lv_obj_update_layout(g_calib_ui.cross);
    int w = lv_obj_get_width(g_calib_ui.cross);
    int h = lv_obj_get_height(g_calib_ui.cross);
    int sw = lv_obj_get_width(scr);
    int sh = lv_obj_get_height(scr);
    int x = pt.x - (w / 2);
    int y = pt.y - (h / 2);
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x > sw - w)
        x = sw - w;
    if (y > sh - h)
        y = sh - h;
    lv_obj_set_pos(g_calib_ui.cross, x, y);
}

static void hide_all_ui()
{
    lv_obj_add_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.disk_missing_1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.disk_missing_2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.boot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.menu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.menu_right, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.clock, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.clock_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.time, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.date, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.temp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.gauge_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.gauge_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.gauge_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.white_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.black_line, LV_OBJ_FLAG_HIDDEN);
    for (size_t i = 0; i < k_plugin_max; ++i)
    {
        if (g_ui.plugin_icons[i])
            lv_obj_add_flag(g_ui.plugin_icons[i], LV_OBJ_FLAG_HIDDEN);
    }
    datetime_ui_hide();
    if (g_calib_ui.label)
        lv_obj_add_flag(g_calib_ui.label, LV_OBJ_FLAG_HIDDEN);
    if (g_calib_ui.cross)
        lv_obj_add_flag(g_calib_ui.cross, LV_OBJ_FLAG_HIDDEN);
}

static lv_draw_buf_t *load_png_once(const char *path)
{
    lv_image_decoder_dsc_t dsc;
    if (lv_image_decoder_open(&dsc, path, NULL) != LV_RESULT_OK)
        return NULL;

    lv_draw_buf_t *dup = lv_draw_buf_dup(dsc.decoded);
    lv_image_decoder_close(&dsc);
    return dup;
}

static void replace_black_with_red(lv_draw_buf_t *buf)
{
    if (!buf)
        return;

    const lv_color_format_t cf = (lv_color_format_t)buf->header.cf;
    const uint32_t w = buf->header.w;
    const uint32_t h = buf->header.h;

    if (LV_COLOR_FORMAT_IS_INDEXED(cf))
    {
        const uint32_t palette_size = LV_COLOR_INDEXED_PALETTE_SIZE(cf);
        lv_color32_t *palette = (lv_color32_t *)buf->data;
        for (uint32_t i = 0; i < palette_size; ++i)
        {
            const lv_color32_t c = palette[i];
            if (c.red <= 8 && c.green <= 8 && c.blue <= 8 && c.alpha > 0)
                lv_draw_buf_set_palette(buf, (uint8_t)i, lv_color32_make(255, 0, 0, c.alpha));
        }
        return;
    }

    if (cf == LV_COLOR_FORMAT_RGB565)
    {
        const uint16_t red_565 = (uint16_t)0xF800;
        uint8_t *row = buf->data;
        const uint32_t stride = buf->header.stride;
        for (uint32_t y = 0; y < h; ++y)
        {
            uint16_t *px = (uint16_t *)row;
            for (uint32_t x = 0; x < w; ++x)
            {
                if (px[x] == 0x0000)
                    px[x] = red_565;
            }
            row += stride;
        }
        return;
    }

    if (cf == LV_COLOR_FORMAT_ARGB8888 || cf == LV_COLOR_FORMAT_XRGB8888 || cf == LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED)
    {
        uint8_t *row = buf->data;
        const uint32_t stride = buf->header.stride;
        for (uint32_t y = 0; y < h; ++y)
        {
            uint8_t *px = row;
            for (uint32_t x = 0; x < w; ++x)
            {
                uint8_t *b = &px[x * 4 + 0];
                uint8_t *g = &px[x * 4 + 1];
                uint8_t *r = &px[x * 4 + 2];
                uint8_t *a = &px[x * 4 + 3];
                if (*r <= 8 && *g <= 8 && *b <= 8 && *a > 0)
                {
                    *b = 0;
                    *g = 0;
                    *r = (cf == LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED) ? *a : 255;
                }
            }
            row += stride;
        }
    }
}

static lv_draw_buf_t *make_plugin_missing_buf(lv_draw_buf_t *src)
{
    if (!src)
        return NULL;
    lv_draw_buf_t *dup = lv_draw_buf_dup(src);
    if (!dup)
        return NULL;
    replace_black_with_red(dup);
    return dup;
}

static void set_image_src(lv_obj_t *img, lv_draw_buf_t *buf, const char *path)
{
    if (buf)
        lv_image_set_src(img, (const lv_image_dsc_t *)buf);
    else
        lv_image_set_src(img, path);
}

static bool littlefs_exists(const char *path)
{
    return LittleFS.exists(path);
}

static void update_clock_labels()
{
    static int last_sec = -1;
    static int16_t gauge_width = 0;
    static int16_t gauge_box_w = 0;
    DateTime now = rtc.now();
    int sec = now.second();
    if (sec == last_sec)
        return;
    last_sec = sec;

    char buf[24];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             now.hour(), now.minute(), sec);
    lv_label_set_text(g_ui.time, buf);
    lv_obj_align(g_ui.time, LV_ALIGN_TOP_MID, 0, 14 + 4);

    int year = now.year();
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d",
             now.day(), now.month(), year);
    lv_label_set_text(g_ui.date, buf);
    lv_obj_align(g_ui.date, LV_ALIGN_TOP_MID, 0, 14 + 4 + 32 + 16);

    if (!bmp.dataReady())
    {
        bmp.performReading();
        char tbuf[12];
        snprintf(tbuf, sizeof(tbuf), "%02.1f°C", bmp.temperature);
        lv_label_set_text(g_ui.temp, tbuf);

        const float p = bmp.pressure;
        if (p < 1000.0f)
            lv_image_set_src(g_ui.gauge_icon, "S:/rainy.png");
        else if (p < 1020.0f)
            lv_image_set_src(g_ui.gauge_icon, "S:/cloudy.png");
        else
            lv_image_set_src(g_ui.gauge_icon, "S:/sunny.png");

        if (gauge_width == 0)
        {
            gauge_width = lv_obj_get_width(g_ui.gauge_line);
            gauge_box_w = lv_obj_get_width(g_ui.gauge_box);
        }

        const float min_p = 980.0f;
        const float max_p = 1040.0f;
        float clamped = p;
        if (clamped < min_p)
            clamped = min_p;
        if (clamped > max_p)
            clamped = max_p;
        const float t = (clamped - min_p) / (max_p - min_p);
        const int16_t max_offset = gauge_width - gauge_box_w;
        const int16_t x_offset = (int16_t)(max_offset * t);
        lv_obj_align_to(g_ui.gauge_box, g_ui.gauge_line, LV_ALIGN_LEFT_MID, x_offset, 0);
        lv_obj_move_foreground(g_ui.gauge_box);
    }
}

static void init_ui_assets()
{
    lv_obj_t *scr = lv_screen_active();
    g_ui.background = lv_image_create(scr);
    g_ui.background_buf = load_png_once("S:/background.png");
    g_ui.corners_buf = load_png_once("S:/corners.png");
    g_ui.disk_missing_1_buf = load_png_once("S:/disk_missing_1.png");
    g_ui.disk_missing_2_buf = load_png_once("S:/disk_missing_2.png");
    g_ui.boot_buf = load_png_once("S:/boot.png");
    g_ui.menu_buf = load_png_once("S:/menu.png");
    g_ui.menu_right_buf = load_png_once("S:/menu_right.png");
    g_ui.icon_buf = load_png_once("S:/icon.png");
    g_ui.clock_buf = load_png_once("S:/empty.png");
    g_ui.plugin_buf = load_png_once("S:/plugin.png");
    g_ui.plugin_missing_buf = make_plugin_missing_buf(g_ui.plugin_buf);

    set_image_src(g_ui.background, g_ui.background_buf, "S:/background.png");
    lv_obj_center(g_ui.background);

    g_ui.white_bar = lv_obj_create(scr);
    lv_obj_remove_style_all(g_ui.white_bar);
    lv_obj_set_size(g_ui.white_bar, lv_pct(100), 19);
    lv_obj_set_pos(g_ui.white_bar, 0, 0);
    lv_obj_set_style_bg_color(g_ui.white_bar, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_ui.white_bar, LV_OPA_COVER, 0);

    g_ui.black_line = lv_obj_create(scr);
    lv_obj_remove_style_all(g_ui.black_line);
    lv_obj_set_size(g_ui.black_line, lv_pct(100), 1);
    lv_obj_set_pos(g_ui.black_line, 0, 19);
    lv_obj_set_style_bg_color(g_ui.black_line, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_ui.black_line, LV_OPA_COVER, 0);

    g_ui.menu = lv_image_create(scr);
    set_image_src(g_ui.menu, g_ui.menu_buf, "S:/menu.png");

    g_ui.menu_right = lv_image_create(scr);
    set_image_src(g_ui.menu_right, g_ui.menu_right_buf, "S:/menu_right.png");
    lv_obj_align(g_ui.menu_right, LV_ALIGN_TOP_RIGHT, 0, 0);

    g_ui.icon = lv_image_create(scr);
    set_image_src(g_ui.icon, g_ui.icon_buf, "S:/icon.png");
    lv_obj_align(g_ui.icon, LV_ALIGN_TOP_RIGHT, -10, 30);

    g_ui.clock = lv_image_create(scr);
    set_image_src(g_ui.clock, g_ui.clock_buf, "S:/clock.png");
    lv_obj_center(g_ui.clock);

    g_ui.clock_label = lv_label_create(g_ui.clock);
    lv_label_set_text(g_ui.clock_label, "Clock");
    lv_obj_set_style_text_font(g_ui.clock_label, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_letter_space(g_ui.clock_label, 1, 0);
    lv_obj_set_style_bg_color(g_ui.clock_label, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_ui.clock_label, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(g_ui.clock_label, 12, 0);
    lv_obj_set_style_pad_right(g_ui.clock_label, 12, 0);
    lv_obj_align(g_ui.clock_label, LV_ALIGN_TOP_MID, 0, 3);

    g_ui.time = lv_label_create(g_ui.clock);
    lv_label_set_text(g_ui.time, "00:00:00");
    lv_obj_set_style_text_font(g_ui.time, &lv_font_chicago_48, 0);
    lv_obj_set_style_text_letter_space(g_ui.time, 1, 0);
    lv_obj_align(g_ui.time, LV_ALIGN_TOP_MID, 0, 8);

    g_ui.date = lv_label_create(g_ui.clock);
    lv_label_set_text(g_ui.date, "1/1/00");
    lv_obj_set_style_text_font(g_ui.date, &lv_font_chicago_32, 0);
    lv_obj_set_style_text_letter_space(g_ui.date, 1, 0);
    lv_obj_align(g_ui.date, LV_ALIGN_TOP_MID, 0, 83);

    g_ui.temp = lv_label_create(g_ui.clock);
    lv_label_set_text(g_ui.temp, "00.0°C");
    lv_obj_set_style_text_font(g_ui.temp, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_letter_space(g_ui.temp, 1, 0);
    lv_obj_align(g_ui.temp, LV_ALIGN_TOP_LEFT, 12, 118);

    g_ui.gauge_icon = lv_image_create(g_ui.clock);
    lv_image_set_src(g_ui.gauge_icon, "S:/cloudy.png");
    lv_obj_align(g_ui.gauge_icon, LV_ALIGN_TOP_RIGHT, -12, 111);

    g_ui.gauge_line = lv_obj_create(g_ui.clock);
    lv_obj_remove_style_all(g_ui.gauge_line);
    lv_obj_set_size(g_ui.gauge_line, 180, 2);
    lv_obj_set_style_bg_color(g_ui.gauge_line, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_ui.gauge_line, LV_OPA_COVER, 0);
    lv_obj_align(g_ui.gauge_line, LV_ALIGN_TOP_RIGHT, -12, 127);

    g_ui.gauge_box = lv_obj_create(g_ui.clock);
    lv_obj_remove_style_all(g_ui.gauge_box);
    lv_obj_set_size(g_ui.gauge_box, 10, 10);
    lv_obj_set_style_border_color(g_ui.gauge_box, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_ui.gauge_box, 1, 0);
    lv_obj_set_style_bg_color(g_ui.gauge_box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_ui.gauge_box, LV_OPA_COVER, 0);
    lv_obj_align(g_ui.gauge_box, LV_ALIGN_TOP_RIGHT, -12, 124);

    g_ui.disk_missing_1 = lv_image_create(scr);
    set_image_src(g_ui.disk_missing_1, g_ui.disk_missing_1_buf, "S:/disk_missing_1.png");
    lv_obj_center(g_ui.disk_missing_1);

    g_ui.disk_missing_2 = lv_image_create(scr);
    set_image_src(g_ui.disk_missing_2, g_ui.disk_missing_2_buf, "S:/disk_missing_2.png");
    lv_obj_center(g_ui.disk_missing_2);

    g_ui.boot = lv_image_create(scr);
    set_image_src(g_ui.boot, g_ui.boot_buf, "S:/boot.png");
    lv_obj_center(g_ui.boot);

    for (size_t i = 0; i < k_plugin_max; ++i)
    {
        g_ui.plugin_icons[i] = lv_image_create(scr);
        set_image_src(g_ui.plugin_icons[i], g_ui.plugin_buf, "S:/plugin.png");
    }

    g_ui.corners = lv_image_create(scr);
    set_image_src(g_ui.corners, g_ui.corners_buf, "S:/corners.png");
    lv_obj_center(g_ui.corners);

    datetime_ui_init(scr);

    g_calib_ui.label = lv_label_create(scr);
    lv_label_set_text(g_calib_ui.label, "Touch the crosshair");
    lv_obj_set_style_text_font(g_calib_ui.label, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_letter_space(g_calib_ui.label, 1, 0);
    lv_obj_align(g_calib_ui.label, LV_ALIGN_TOP_MID, 0, 24);

    g_calib_ui.cross = lv_label_create(scr);
    lv_label_set_text(g_calib_ui.cross, "+");
    lv_obj_set_style_text_font(g_calib_ui.cross, &lv_font_chicago_32, 0);

    g_cursor = lv_image_create(scr);
    lv_image_set_src(g_cursor, "S:/cursor.png");
    lv_obj_clear_flag(g_cursor, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_cursor, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(scr, screen_touch_event, LV_EVENT_PRESSED, NULL);

    hide_all_ui();
}

static InputState read_input_state()
{
    InputState snapshot;
    portENTER_CRITICAL(&g_input_state_mux);
    snapshot = g_input_state;
    g_input_state.alarm = g_input_state.clock = g_input_state.touch = false;
    portEXIT_CRITICAL(&g_input_state_mux);
    return snapshot;
}

static void input_task(void *param)
{
    (void)param;
    static bool last_alarm = false;
    static bool last_clock = false;
    static bool last_touch = false;
    for (;;)
    {
        bool floppy = !digitalRead(GPIO_FLOPPY);
        bool alarm = !digitalRead(GPIO_ALARM);
        bool clock = !digitalRead(GPIO_CLOCK);
        bool touched = touch.update();
        portENTER_CRITICAL(&g_input_state_mux);
        g_input_state.floppy = floppy;
        if (alarm && !last_alarm)
            g_input_state.alarm = true;
        if (clock && !last_clock)
            g_input_state.clock = true;
        if (touched && !last_touch)
            g_input_state.touch = true;
        portEXIT_CRITICAL(&g_input_state_mux);

        last_alarm = alarm;
        last_clock = clock;
        last_touch = touched;

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void audio_task(void *param)
{
    (void)param;
    for (;;)
    {
        if (mp3 && mp3->isRunning())
        {
            if (!mp3->loop())
            {
                mp3->stop();
                portENTER_CRITICAL(&g_mp3_mux);
                g_mp3_finished = true;
                portEXIT_CRITICAL(&g_mp3_mux);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static bool i2c_device_present(uint8_t addr)
{
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

void setup()
{
    Serial.begin(115200);
    analogWrite(TFT_BL_VAR, 0);

    heap_caps_malloc_extmem_enable(0);
    LittleFS.begin();
    my_lcd.init();
    touch_eeprom_begin();
    touch_load_calibration();
    touch.begin();

    my_lcd.setAddrWindow(0, 0, LCD_W, LCD_H);
    my_lcd.fillScreen(TFT_BLACK);
    my_lcd.setRotation(3);

    touch_init(my_lcd.width(), my_lcd.height(), my_lcd.getRotation());

    pinMode(GPIO_FLOPPY, INPUT);

    if (!digitalRead(GPIO_FLOPPY)) {
        analogWrite(TFT_BL_VAR, 255);
        minivmac();
    }

    setup_codec();
    setup_lvgl_display();
    setup_lvgl_input();
    lvgl_fs_init_littlefs();
    init_ui_assets();
    Wire.begin(I2C_SDA, I2C_SCL);
    rtc.begin();

    pinMode(GPIO_ALARM, INPUT);
    pinMode(GPIO_CLOCK, INPUT);
    pinMode(GPIO_ENCODER1, INPUT_PULLUP);
    pinMode(GPIO_ENCODER2, INPUT_PULLUP);
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachHalfQuad(GPIO_ENCODER1, GPIO_ENCODER2);
    {
        uint8_t saved = rtc.readnvram(k_nvram_addr_encoder);
        int start_count = (saved <= 12) ? saved : 6;
        encoder.setCount(start_count);
    }

    pinMode(GPIO_CHARGING, INPUT_PULLDOWN);
    pinMode(GPIO_BAT_EN, OUTPUT);
    digitalWrite(GPIO_BAT_EN, 1);

    xTaskCreatePinnedToCore(
        input_task,
        "input_task",
        2048,
        nullptr,
        1,
        nullptr,
        1);

    xTaskCreatePinnedToCore(
        audio_task,
        "audio_task",
        4096,
        nullptr,
        1,
        nullptr,
        0);
    bmp.begin(BMP5XX_ALTERNATIVE_ADDRESS, &Wire);
    bmp.setTemperatureOversampling(BMP5XX_OVERSAMPLING_16X);
    bmp.setPressureOversampling(BMP5XX_OVERSAMPLING_16X);
    bmp.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_127);
    bmp.setOutputDataRate(BMP5XX_ODR_120_HZ);
    bmp.setPowerMode(BMP5XX_POWERMODE_NORMAL);
    bmp.enablePressure(true);

    if (!digitalRead(GPIO_CLOCK)) // run calibration if clock button set on boot
        request_state(UI_STATE_CALIBRATION);
}

void loop()
{
    static int currentState = UI_STATE_EMPTY_SCREEN;
    static unsigned long stateStartTime = 0;
    static int lastState = -1;
    static uint8_t plugin_count = 0;
    static uint8_t plugin_reveal = 0;
    static unsigned long plugin_next_reveal = 0;
    static int calib_step = 0;
    static bool calib_wait_release = false;
    static uint16_t calib_raw_x[4] = {};
    static uint16_t calib_raw_y[4] = {};
    static lv_point_t calib_targets[4] = {};
    static int last_saved_encoder = -1;
    static unsigned long last_encoder_save_ms = 0;
    static unsigned long full_brightness_until = 0;

    unsigned long now = millis();
    InputState inputs = read_input_state();

    if (g_requested_state != 0)
    {
        currentState = g_requested_state;
        stateStartTime = now;
        g_requested_state = 0;
    }

    switch (currentState)
    {
    case UI_STATE_EMPTY_SCREEN: //  empty screen, start sound
        if (now - stateStartTime >= 0)
        {
            hide_all_ui();
            lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
            lv_timer_handler();
            if (!mp3 || !mp3->isRunning())
            {
                file = new AudioFileSourceLittleFS("/startup.mp3");
                mp3 = new AudioGeneratorMP3();
                mp3->begin(file, audio_out);
                portENTER_CRITICAL(&g_mp3_mux);
                g_mp3_finished = false;
                portEXIT_CRITICAL(&g_mp3_mux);
            }
            g_requested_state = currentState + 1;
            stateStartTime = now;
        }
        break;
    case UI_STATE_WAIT_STARTUP_SOUND: // wait for end of startup sound
    {
        bool finished = false;
        portENTER_CRITICAL(&g_mp3_mux);
        finished = g_mp3_finished;
        g_mp3_finished = false;
        portEXIT_CRITICAL(&g_mp3_mux);
        if (finished)
        {
            g_requested_state = currentState + 1;
            stateStartTime = now;
        }
    }
    break;
    case UI_STATE_WAIT_FLOPPY_1: // wait for floppy 1
        if (now - stateStartTime >= 1000)
        {
            hide_all_ui();
            lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.disk_missing_1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
            lv_timer_handler();
            g_requested_state = currentState + 1;
            stateStartTime = now;
        }
        if (inputs.floppy)
        {
            currentState += 2;
            stateStartTime = now;
        }
        break;
    case UI_STATE_WAIT_FLOPPY_2: // wait for floppy 2
        if (now - stateStartTime >= 1000)
        {
            hide_all_ui();
            lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.disk_missing_2, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
            lv_timer_handler();

            currentState--;
            stateStartTime = now;
        }
        if (inputs.floppy)
        {
            g_requested_state = currentState + 1;
            stateStartTime = now;
        }
        break;
    case UI_STATE_FLOPPY_INSERTED: // floppy inserted, loading...
    {
        hide_all_ui();
        lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
        lv_timer_handler();
        file = new AudioFileSourceLittleFS("/floppy.mp3");
        mp3 = new AudioGeneratorMP3();
        mp3->begin(file, audio_out);
        es8311_voice_volume_set(es8311_handle, 65, NULL);
        portENTER_CRITICAL(&g_mp3_mux);
        g_mp3_finished = false;
        portEXIT_CRITICAL(&g_mp3_mux);
    }
        g_requested_state = currentState + 1;
        stateStartTime = now;
        break;
    case UI_STATE_BOOT_PLUGINS: // show boot screen + detected i2c plugins
        if (currentState != lastState)
        {
            static const uint8_t k_addrs[k_plugin_max] = {0x18, 0x38, 0x47, 0x50, 0x68};
            const int16_t margin_x = 8;
            const int16_t margin_y = 8;
            const int16_t spacing = 4;
            const int16_t icon_size = 32;
            plugin_count = 0;
            plugin_reveal = 0;
            plugin_next_reveal = now + (unsigned long)random(100, 301);
            for (size_t i = 0; i < k_plugin_max; ++i)
            {
                const uint8_t addr = k_addrs[i];
                if (i2c_device_present(addr) && plugin_count < k_plugin_max)
                {
                    char fs_path[32];
                    char lv_path[36];
                    snprintf(fs_path, sizeof(fs_path), "/plugin_0x%02X.png", addr);
                    snprintf(lv_path, sizeof(lv_path), "S:/plugin_0x%02X.png", addr);
                    if (littlefs_exists(fs_path))
                        lv_image_set_src(g_ui.plugin_icons[plugin_count], lv_path);
                    else
                        set_image_src(g_ui.plugin_icons[plugin_count], g_ui.plugin_buf, "S:/plugin.png");
                    lv_obj_align(g_ui.plugin_icons[plugin_count], LV_ALIGN_BOTTOM_LEFT,
                                 margin_x + (int16_t)plugin_count * (icon_size + spacing),
                                 -margin_y);
                    plugin_count++;
                }
                else
                {
                    char fs_path[32];
                    char lv_path[36];
                    snprintf(fs_path, sizeof(fs_path), "/plugin_0x%02X.png", addr);
                    snprintf(lv_path, sizeof(lv_path), "S:/plugin_0x%02X.png", addr);
                    if (littlefs_exists(fs_path))
                    {
                        lv_draw_buf_t *missing = make_plugin_missing_buf(load_png_once(lv_path));
                        set_image_src(g_ui.plugin_icons[plugin_count], missing, lv_path);
                    }
                    else
                    {
                        set_image_src(g_ui.plugin_icons[plugin_count], g_ui.plugin_missing_buf, "S:/plugin.png");
                    }
                    lv_obj_align(g_ui.plugin_icons[plugin_count], LV_ALIGN_BOTTOM_LEFT,
                                 margin_x + (int16_t)plugin_count * (icon_size + spacing),
                                 -margin_y);
                    hide_all_ui();
                    lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(g_ui.boot, LV_OBJ_FLAG_HIDDEN);
                    for (size_t j = 0; j < plugin_count; ++j)
                    {
                        lv_obj_clear_flag(g_ui.plugin_icons[j], LV_OBJ_FLAG_HIDDEN);
                    }
                    lv_obj_clear_flag(g_ui.plugin_icons[plugin_count], LV_OBJ_FLAG_HIDDEN);
                    lv_timer_handler();
                    bool blink_on = true;
                    for (;;)
                    {
                        if (blink_on)
                            lv_obj_clear_flag(g_ui.plugin_icons[plugin_count], LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_add_flag(g_ui.plugin_icons[plugin_count], LV_OBJ_FLAG_HIDDEN);
                        blink_on = !blink_on;
                        lv_timer_handler();
                        vTaskDelay(pdMS_TO_TICKS(500));
                    }
                }
            }
            for (size_t i = plugin_count; i < k_plugin_max; ++i)
                lv_obj_add_flag(g_ui.plugin_icons[i], LV_OBJ_FLAG_HIDDEN);
        }
        if (now - stateStartTime >= 0)
        {
            hide_all_ui();
            lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.boot, LV_OBJ_FLAG_HIDDEN);
            if (plugin_reveal < plugin_count && now >= plugin_next_reveal)
            {
                plugin_reveal++;
                plugin_next_reveal = now + (unsigned long)random(200, 600);
            }
            for (size_t i = 0; i < plugin_reveal; ++i)
                lv_obj_clear_flag(g_ui.plugin_icons[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
            lv_timer_handler();
        }
        if (now - stateStartTime >= 1500 && plugin_reveal == plugin_count)
        {
            g_requested_state = currentState + 1;
            stateStartTime = now;
        }
        break;
    case UI_STATE_WAIT_FLOPPY_SOUND: // wait for end of floppy sound
    {
        bool finished = false;
        portENTER_CRITICAL(&g_mp3_mux);
        finished = g_mp3_finished;
        g_mp3_finished = false;
        portEXIT_CRITICAL(&g_mp3_mux);
        if (finished)
        {
            g_requested_state = currentState + 1;
            stateStartTime = now;
        }
    }
    break;
    case UI_STATE_NORMAL: // normal state
    {
        static unsigned long lastClockUpdate = 0;
        if (currentState != lastState)
        {
            hide_all_ui();
            lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.white_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.black_line, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.menu, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.menu_right, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.clock, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.clock_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.time, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.date, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.temp, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.gauge_icon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.gauge_line, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.gauge_box, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
            lv_timer_handler();
        }
        if (!lastClockUpdate || now - lastClockUpdate >= 100)
        {
            if (inputs.floppy)
                lv_obj_clear_flag(g_ui.icon, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(g_ui.icon, LV_OBJ_FLAG_HIDDEN);
            update_clock_labels();
            lv_timer_handler();
            lastClockUpdate = now;
        }
        if (inputs.clock)
        {
            g_requested_state = UI_STATE_SET_DATETIME;
            stateStartTime = now;
        }
        break;
    }
    case UI_STATE_SET_DATETIME: // change date/time
        if (currentState != lastState)
        {
            DateTime current = rtc.now();
            datetime_ui_enter(current);
        }
        if (now - stateStartTime >= 0)
        {
            hide_all_ui();
            lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.white_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.black_line, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
            datetime_ui_show();
            lv_timer_handler();
        }
        break;
    case UI_STATE_CALIBRATION: // calibration screen
        if (currentState != lastState)
        {
            lv_obj_t *scr = lv_screen_active();
            int w = lv_obj_get_width(scr);
            int h = lv_obj_get_height(scr);
            int margin = 16;
            calib_targets[0] = {margin, margin};
            calib_targets[1] = {w - margin, margin};
            calib_targets[2] = {w - margin, h - margin};
            calib_targets[3] = {margin, h - margin};
            calib_step = 0;
            calib_wait_release = false;
            lv_label_set_text(g_calib_ui.label, "Touch the crosshair");
            calib_set_cross_pos(calib_targets[0]);
        }
        if (now - stateStartTime >= 0)
        {
            hide_all_ui();
            lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_calib_ui.label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_calib_ui.cross, LV_OBJ_FLAG_HIDDEN);
            lv_timer_handler();
        }
        {
            uint16_t raw_x = 0;
            uint16_t raw_y = 0;
            bool touched = touch_read_raw(raw_x, raw_y);
            if (touched && !calib_wait_release)
            {
                calib_raw_x[calib_step] = raw_x;
                calib_raw_y[calib_step] = raw_y;
                calib_wait_release = true;
            }
            if (!touched && calib_wait_release)
            {
                calib_wait_release = false;
                calib_step++;
                if (calib_step < 4)
                {
                    calib_set_cross_pos(calib_targets[calib_step]);
                }
                else
                {
                    uint16_t minx = min(calib_raw_x[0], calib_raw_x[3]);
                    uint16_t maxx = max(calib_raw_x[1], calib_raw_x[2]);
                    uint16_t miny = min(calib_raw_y[0], calib_raw_y[1]);
                    uint16_t maxy = max(calib_raw_y[2], calib_raw_y[3]);
                    touch_set_calibration(minx, maxx, miny, maxy);
                    touch_save_calibration();
                    g_requested_state = UI_STATE_NORMAL;
                    stateStartTime = now;
                }
            }
        }
        break;
    }

    lastState = currentState;

    if (inputs.touch)
        full_brightness_until = now + 10000;

    int enc = encoder.getCount();
    if (enc < 0)
        enc = 0;
    if (enc > 12)
        enc = 12;
    if (enc != encoder.getCount())
        encoder.setCount(enc);
    if (now < full_brightness_until)
        analogWrite(TFT_BL_VAR, 255);
    else
        analogWrite(TFT_BL_VAR, enc * 255 / 12);

    if (enc != last_saved_encoder && (now - last_encoder_save_ms) >= 500)
    {
        rtc.writenvram(k_nvram_addr_encoder, (uint8_t)enc);
        last_saved_encoder = enc;
        last_encoder_save_ms = now;
    }
}

