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
#include "datetime_ui.h"

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

struct InputState
{
    bool floppy;
    bool alarm;
    bool clock;
    bool touch;
};

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
    lv_obj_t *white_bar;
    lv_obj_t *black_line;

    lv_draw_buf_t *background_buf;
    lv_draw_buf_t *corners_buf;
    lv_draw_buf_t *disk_missing_1_buf;
    lv_draw_buf_t *disk_missing_2_buf;
    lv_draw_buf_t *boot_buf;
    lv_draw_buf_t *menu_buf;
    lv_draw_buf_t *menu_right_buf;
    lv_draw_buf_t *icon_buf;
    lv_draw_buf_t *clock_buf;
};

static InputState g_input_state = {};
static portMUX_TYPE g_input_state_mux = portMUX_INITIALIZER_UNLOCKED;
static UiImages g_ui = {};
static bool g_mp3_finished = false;
static portMUX_TYPE g_mp3_mux = portMUX_INITIALIZER_UNLOCKED;
static int g_requested_state = 0;
static lv_obj_t *g_cursor = nullptr;
static lv_timer_t *g_cursor_timer = nullptr;

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
    lv_obj_add_flag(g_ui.white_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.black_line, LV_OBJ_FLAG_HIDDEN);
    datetime_ui_hide();
}

static void show_ui(lv_obj_t *obj)
{
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
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

static void set_image_src(lv_obj_t *img, lv_draw_buf_t *buf, const char *path)
{
    if (buf)
        lv_image_set_src(img, (const lv_image_dsc_t *)buf);
    else
        lv_image_set_src(img, path);
}

static void update_clock_labels()
{
    static int last_sec = -1;
    DateTime now = rtc.now();
    int sec = now.second();
    if (sec == last_sec)
        return;
    last_sec = sec;

    char buf[24];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             now.hour(), now.minute(), sec);
    lv_label_set_text(g_ui.time, buf);
    lv_obj_align(g_ui.time, LV_ALIGN_TOP_MID, 0, 19 + 4);

    int year = now.year();
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d",
             now.day(), now.month(), year);
    lv_label_set_text(g_ui.date, buf);
    lv_obj_align(g_ui.date, LV_ALIGN_TOP_MID, 0, 19 + 4 + 32 + 16 + 16);
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

    g_ui.disk_missing_1 = lv_image_create(scr);
    set_image_src(g_ui.disk_missing_1, g_ui.disk_missing_1_buf, "S:/disk_missing_1.png");
    lv_obj_center(g_ui.disk_missing_1);

    g_ui.disk_missing_2 = lv_image_create(scr);
    set_image_src(g_ui.disk_missing_2, g_ui.disk_missing_2_buf, "S:/disk_missing_2.png");
    lv_obj_center(g_ui.disk_missing_2);

    g_ui.boot = lv_image_create(scr);
    set_image_src(g_ui.boot, g_ui.boot_buf, "S:/boot.png");
    lv_obj_center(g_ui.boot);

    g_ui.corners = lv_image_create(scr);
    set_image_src(g_ui.corners, g_ui.corners_buf, "S:/corners.png");
    lv_obj_center(g_ui.corners);

    datetime_ui_init(scr);

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
    g_input_state = {};
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
        if (floppy)
            g_input_state.floppy = true;
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

static void scan_i2c()
{
    Serial.println("I2C scan start");
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; ++addr)
    {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0)
        {
            Serial.print("I2C device @ 0x");
            if (addr < 16)
                Serial.print('0');
            Serial.println(addr, HEX);
            ++found;
        }
    }
    Serial.print("I2C scan done, devices found: ");
    Serial.println(found);
}

void setup_codec();
void setup_lvgl_display();
void setup_lvgl_input();
void lvgl_fs_init_littlefs();

void setup()
{
    Serial.begin(115200);
    analogWrite(TFT_BL_VAR, 0);

    heap_caps_malloc_extmem_enable(0);
    setup_codec();
    setup_lvgl_display();
    setup_lvgl_input();
    lvgl_fs_init_littlefs();
    init_ui_assets();
    Wire.begin(I2C_SDA, I2C_SCL);
    rtc.begin();

    pinMode(GPIO_FLOPPY, INPUT);
    pinMode(GPIO_ALARM, INPUT);
    pinMode(GPIO_CLOCK, INPUT);
    pinMode(GPIO_ENCODER1, INPUT_PULLUP);
    pinMode(GPIO_ENCODER2, INPUT_PULLUP);
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachHalfQuad(GPIO_ENCODER1, GPIO_ENCODER2);
    encoder.setCount(25);

    touch.begin();
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
    scan_i2c();
}

void loop()
{
    static int currentState = 1;
    static unsigned long stateStartTime = 0;
    static int lastState = -1;
    unsigned long now = millis();
    InputState inputs = read_input_state();

    if (g_requested_state != 0)
    {
        currentState = g_requested_state;
        stateStartTime = now;
        g_requested_state = 0;
    }

    if (currentState != lastState)
    {
        if (currentState == 8)
        {
            DateTime current = rtc.now();
            datetime_ui_enter(current);
        }
        lastState = currentState;
    }

    switch (currentState)
    {
    case 1:
        if (now - stateStartTime >= 0) // empty screen
        {
            hide_all_ui();
            show_ui(g_ui.background);
            show_ui(g_ui.corners);
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

            currentState++;
            stateStartTime = now;
        }
        break;

    case 2:
        if (now - stateStartTime >= 1000)
        {
            hide_all_ui();
            show_ui(g_ui.background);
            show_ui(g_ui.disk_missing_1);
            show_ui(g_ui.corners);
            lv_timer_handler();
            currentState++;
            stateStartTime = now;
        }
        if (inputs.floppy)
        {
            currentState += 2;
            stateStartTime = now;
        }
        break;

    case 3:
        if (now - stateStartTime >= 1000)
        {
            hide_all_ui();
            show_ui(g_ui.background);
            show_ui(g_ui.disk_missing_2);
            show_ui(g_ui.corners);
            lv_timer_handler();

            currentState--;
            stateStartTime = now;
        }
        if (inputs.floppy)
        {
            currentState++;
            stateStartTime = now;
        }
        break;
    case 4:
    {
        hide_all_ui();
        show_ui(g_ui.background);
        show_ui(g_ui.corners);
        lv_timer_handler();
        file = new AudioFileSourceLittleFS("/floppy.mp3");
        mp3 = new AudioGeneratorMP3();
        mp3->begin(file, audio_out);
        es8311_voice_volume_set(es8311_handle, 65, NULL);
        portENTER_CRITICAL(&g_mp3_mux);
        g_mp3_finished = false;
        portEXIT_CRITICAL(&g_mp3_mux);
    }
        currentState++;
        stateStartTime = now;
        break;
    case 5:
        if (now - stateStartTime >= 3000)
        {
            hide_all_ui();
            show_ui(g_ui.background);
            show_ui(g_ui.boot);
            show_ui(g_ui.corners);
            lv_timer_handler();
            currentState++;
            stateStartTime = now;
        }
        break;
    case 6:
    {
        bool finished = false;
        portENTER_CRITICAL(&g_mp3_mux);
        finished = g_mp3_finished;
        g_mp3_finished = false;
        portEXIT_CRITICAL(&g_mp3_mux);
        if (finished)
        {
            currentState++;
            stateStartTime = now;
        }
    }
        break;
    case 7:
        if (now - stateStartTime >= 1000)
        {
            hide_all_ui();
            show_ui(g_ui.background);
            show_ui(g_ui.white_bar);
            show_ui(g_ui.black_line);
            show_ui(g_ui.menu);
            show_ui(g_ui.menu_right);
            show_ui(g_ui.clock);
            show_ui(g_ui.clock_label);
            show_ui(g_ui.time);
            show_ui(g_ui.date);
            show_ui(g_ui.corners);
            if (inputs.floppy)
                show_ui(g_ui.icon);
            update_clock_labels();
            lv_timer_handler();
        }
        if (inputs.clock)
        {
            currentState = 8;
            stateStartTime = now;
        }
        break;
    case 8:
        if (now - stateStartTime >= 0)
        {
            hide_all_ui();
            show_ui(g_ui.background);
            show_ui(g_ui.white_bar);
            show_ui(g_ui.black_line);
            show_ui(g_ui.corners);
            datetime_ui_show();
            lv_timer_handler();
        }
        break;
    }

    if (inputs.touch || inputs.alarm || inputs.clock)
    {
        Serial.print(inputs.touch);
        Serial.print(inputs.floppy);
        Serial.print(inputs.alarm);
        Serial.println(inputs.clock);
    }

    if (encoder.getCount() < 0)
        encoder.setCount(0);
    if (encoder.getCount() > 12)
        encoder.setCount(12);
    analogWrite(TFT_BL_VAR, encoder.getCount() * 255 / 12);
}
