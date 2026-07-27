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
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/touch_pad.h"
#include "TouchSensor.h"
#include <Wire.h>
#include <LittleFS.h>
#include <math.h>
#include "datetime_ui.h"
#include "alarm_ui.h"
#include "timer_ui.h"
#include "touch.h"
#include "Adafruit_BMP5xx.h"
#include "Adafruit_HTU21DF.h"
#include <Preferences.h>
#include "brightness.h"
#include "localization.h"
#include "regional_settings.h"
#include "selector_list_style.h"
#include "sound_selector.h"
#include "wifi_mode.h"

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
RTC_DS1307 rtc_ds1307;
RTC_DS3231 rtc_ds3231;
Adafruit_BMP5xx bmp;
Adafruit_HTU21DF htu2x;
Preferences preferences;

enum RtcType
{
    RTC_TYPE_NONE,
    RTC_TYPE_DS1307,
    RTC_TYPE_DS3231
};

static RtcType g_rtc_type = RTC_TYPE_NONE;

enum WeatherSensor
{
    WEATHER_SENSOR_NONE,
    WEATHER_SENSOR_BMP5XX,
    WEATHER_SENSOR_HTU2X
};

static WeatherSensor g_weather_sensor = WEATHER_SENSOR_NONE;
static uint8_t g_weather_sensor_address = 0;

struct InputState
{
    bool floppy;
    bool alarm;
    bool clock;
    bool touch;
};

static constexpr size_t k_plugin_max = 4;

struct UiImages
{
    lv_obj_t *background;
    lv_obj_t *corners;
    lv_obj_t *disk_missing_1;
    lv_obj_t *disk_missing_2;
    lv_obj_t *boot;
    lv_obj_t *boot_message;
    lv_obj_t *menu;
    lv_obj_t *menu_titles;
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
    lv_obj_t *alarm_indicator;
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
    lv_draw_buf_t *alarm_indicator_buf;
    lv_draw_buf_t *plugin_buf;
    lv_draw_buf_t *plugin_missing_buf;
};

struct CalibUi
{
    lv_obj_t *label;
    lv_obj_t *cross;
};

enum BootOptionsPage
{
    BOOT_OPTIONS_START,
    BOOT_OPTIONS_PREFERENCES,
    BOOT_OPTIONS_LANGUAGE,
    BOOT_OPTIONS_REGIONAL,
    BOOT_OPTIONS_CLOCK_FACE,
    BOOT_OPTIONS_CLOCK_THEME,
    BOOT_OPTIONS_SCREENSAVER,
    BOOT_OPTIONS_NIGHT_SCHEDULE,
    BOOT_OPTIONS_NIGHT_SCREEN,
    BOOT_OPTIONS_CHIME,
    BOOT_OPTIONS_CHIME_SOUND,
    BOOT_OPTIONS_CHIME_VOLUME,
    BOOT_OPTIONS_CHIME_QUIET,
    BOOT_OPTIONS_WIFI,
    BOOT_OPTIONS_TOOLS,
    BOOT_OPTIONS_PAGE_COUNT
};

struct BootOptionsUi
{
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *pages[BOOT_OPTIONS_PAGE_COUNT];
    lv_obj_t *brightness_options;
    lv_obj_t *remember_selection;
    lv_obj_t *language_options;
    lv_obj_t *language_items[UI_LANGUAGE_COUNT];
    lv_obj_t *date_format_options;
    lv_obj_t *temperature_unit_options;
    lv_obj_t *clock_face_options;
    lv_obj_t *clock_theme_options;
    lv_obj_t *screensaver_options;
    lv_obj_t *screensaver_delay_options;
    lv_obj_t *night_enabled_options;
    lv_obj_t *night_start_options;
    lv_obj_t *night_end_options;
    lv_obj_t *night_screen_options;
    lv_obj_t *night_off_options;
    lv_obj_t *chime_mode_options;
    SoundSelector chime_sound_selector;
    lv_obj_t *chime_volume_options;
    lv_obj_t *chime_quiet_options;
    lv_obj_t *chime_quiet_start_options;
    lv_obj_t *chime_quiet_end_options;
    lv_obj_t *wifi_enabled_options;
    lv_obj_t *wifi_status;
    lv_obj_t *rtc_status;
    lv_obj_t *previous;
    lv_obj_t *previous_label;
    lv_obj_t *exit;
    lv_obj_t *exit_label;
    lv_obj_t *next;
    lv_obj_t *next_label;
    lv_obj_t *brightness_label;
    lv_obj_t *remember_label;
    lv_obj_t *date_format_label;
    lv_obj_t *temperature_unit_label;
    lv_obj_t *screensaver_delay_label;
    lv_obj_t *dim_from_label;
    lv_obj_t *normal_at_label;
    lv_obj_t *screen_off_label;
    lv_obj_t *quiet_from_label;
    lv_obj_t *quiet_end_label;
    lv_obj_t *wifi_setup_label;
    lv_obj_t *clock_button_label;
    lv_obj_t *emulator_button_label;
    lv_obj_t *diagnostics_button_label;
    lv_obj_t *calibration_label;
};

enum ClockFace
{
    CLOCK_FACE_MACINTOSH,
    CLOCK_FACE_COMPACT,
    CLOCK_FACE_ANALOG,
    CLOCK_FACE_FLIP,
    CLOCK_FACE_COUNT
};

enum ClockTheme
{
    CLOCK_THEME_LIGHT,
    CLOCK_THEME_DARK,
    CLOCK_THEME_COUNT
};

enum ScreensaverMode
{
    SCREENSAVER_OFF,
    SCREENSAVER_AFTER_DARK,
    SCREENSAVER_MODE_COUNT
};

static constexpr size_t kScreensaverStarCount = 24;
static constexpr size_t kFlipDigitCount = 4;

struct FlipCardAnimation
{
    lv_obj_t *card;
    lv_obj_t *label;
    lv_obj_t *flap;
    lv_obj_t *flap_label;
    char displayed[3];
    char pending[3];
    bool initialized;
    bool animating;
};

struct ClockFacesUi
{
    lv_obj_t *compact;
    lv_obj_t *compact_title;
    lv_obj_t *compact_time;
    lv_obj_t *compact_date;
    lv_obj_t *compact_weather;
    lv_obj_t *analog;
    lv_obj_t *analog_dial;
    lv_obj_t *analog_numbers[12];
    lv_obj_t *analog_hour_hand;
    lv_obj_t *analog_minute_hand;
    lv_obj_t *analog_second_hand;
    lv_obj_t *analog_center;
    lv_point_precise_t analog_hour_points[2];
    lv_point_precise_t analog_minute_points[2];
    lv_point_precise_t analog_second_points[2];
    lv_obj_t *analog_date;
    lv_obj_t *flip;
    lv_obj_t *flip_cards[kFlipDigitCount];
    lv_obj_t *flip_digits[kFlipDigitCount];
    lv_obj_t *flip_colon;
    lv_obj_t *flip_colon_top;
    lv_obj_t *flip_colon_bottom;
    lv_obj_t *flip_title;
    lv_obj_t *flip_date;
    FlipCardAnimation flip_animations[kFlipDigitCount];
    lv_obj_t *screensaver;
    lv_obj_t *screensaver_stars[kScreensaverStarCount];
    int16_t screensaver_star_x[kScreensaverStarCount];
    int16_t screensaver_star_y[kScreensaverStarCount];
    uint8_t screensaver_star_speed[kScreensaverStarCount];
    lv_obj_t *screensaver_clock;
    lv_obj_t *screensaver_time;
    int16_t screensaver_clock_x;
    int16_t screensaver_clock_y;
    int8_t screensaver_clock_dx;
    int8_t screensaver_clock_dy;
};

struct DiagnosticsUi
{
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *status;
    lv_obj_t *back_label;
};

struct WifiSetupUi
{
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *status;
    lv_obj_t *back_label;
};

enum BootBrightness
{
    BOOT_BRIGHTNESS_LATEST,
    BOOT_BRIGHTNESS_LOWEST,
    BOOT_BRIGHTNESS_HIGHEST
};

struct NightModeSettings
{
    bool enabled;
    uint8_t start_hour;
    uint8_t end_hour;
    bool screen_off_enabled;
    uint8_t screen_off_hour;
};

enum NightDisplayState
{
    NIGHT_DISPLAY_NORMAL,
    NIGHT_DISPLAY_DIMMED,
    NIGHT_DISPLAY_OFF
};

enum ChimeMode
{
    CHIME_MODE_OFF,
    CHIME_MODE_HOURLY,
    CHIME_MODE_QUARTER_HOUR,
    CHIME_MODE_COUNT
};

struct ChimeSettings
{
    ChimeMode mode;
    uint8_t sound;
    uint8_t volume;
    bool quiet_enabled;
    uint8_t quiet_start_hour;
    uint8_t quiet_end_hour;
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
    UI_STATE_CALIBRATION = 10,
    UI_STATE_BOOT_OPTIONS = 11,
    UI_STATE_EMULATOR = 12,
    UI_STATE_DIAGNOSTICS = 13,
    UI_STATE_ALARM_EDITOR = 14,
    UI_STATE_ALARM_RINGING = 15,
    UI_STATE_TIMER_EDITOR = 16,
    UI_STATE_TIMER_FINISHED = 17,
    UI_STATE_WIFI_SETUP = 18
};

static InputState g_input_state = {};
static portMUX_TYPE g_input_state_mux = portMUX_INITIALIZER_UNLOCKED;
static UiImages g_ui = {};
static CalibUi g_calib_ui = {};
static BootOptionsUi g_boot_options_ui = {};
static ClockFacesUi g_clock_faces_ui = {};
static DiagnosticsUi g_diagnostics_ui = {};
static WifiSetupUi g_wifi_setup_ui = {};
static bool g_mp3_finished = false;
static portMUX_TYPE g_mp3_mux = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t g_mp3_lock = nullptr;
static int g_requested_state = 0;
static int g_active_alarm_index = -1;
static lv_obj_t *g_cursor = nullptr;
static lv_timer_t *g_cursor_timer = nullptr;
static BootBrightness g_boot_brightness = BOOT_BRIGHTNESS_LATEST;
static UiDateFormat g_date_format = UI_DATE_FORMAT_DMY;
static UiTemperatureUnit g_temperature_unit =
    UI_TEMPERATURE_CELSIUS;
static ClockFace g_clock_face = CLOCK_FACE_MACINTOSH;
static ClockTheme g_clock_theme = CLOCK_THEME_LIGHT;
static ScreensaverMode g_screensaver_mode = SCREENSAVER_OFF;
static uint8_t g_screensaver_delay_index = 1;
static bool g_screensaver_active = false;
static unsigned long g_last_clock_activity_ms = 0;
static BootOptionsPage g_boot_options_page = BOOT_OPTIONS_START;
static bool g_boot_floppy_emulator = true;
static NightModeSettings g_night_mode = {false, 22, 7, false, 23};
static ChimeSettings g_chime = {
    CHIME_MODE_OFF, 0, 1, true, 22, 7};
static int g_last_saved_encoder = -1;
static TaskHandle_t g_input_task_handle = nullptr;
static TaskHandle_t g_audio_task_handle = nullptr;
static char g_night_start_text[6] = "22:00";
static char g_night_end_text[6] = "07:00";
static char g_night_off_text[6] = "23:00";
static const char *g_night_start_map[] = {
    "-", g_night_start_text, "+", ""};
static const char *g_night_end_map[] = {
    "-", g_night_end_text, "+", ""};
static const char *g_night_off_map[] = {
    "-", g_night_off_text, "+", ""};
static char g_chime_quiet_start_text[6] = "22:00";
static char g_chime_quiet_end_text[6] = "07:00";
static const char *g_chime_quiet_start_map[] = {
    "-", g_chime_quiet_start_text, "+", ""};
static const char *g_chime_quiet_end_map[] = {
    "-", g_chime_quiet_end_text, "+", ""};
static const char *g_legacy_chime_sound_paths[] = {
    "/quack.mp3", "/startup.mp3", "/floppy.mp3"};
static char g_chime_sound_path[SOUND_SELECTOR_PATH_MAX] =
    "/quack.mp3";
static const uint8_t g_chime_volumes[] = {25, 50, 75, 100};
static const char *g_brightness_map[4] = {};
static const char *g_remember_map[3] = {};
static const char *g_date_format_map[] = {
    "DD/MM/YYYY", "MM/DD/YYYY", "YYYY-MM-DD", ""};
static const char *g_temperature_unit_map[] = {
    "°C", "°F", ""};
static const char *g_clock_face_map[7] = {};
static const char *g_clock_theme_map[3] = {};
static const char *g_screensaver_map[3] = {};
static const uint8_t g_screensaver_delays_minutes[] = {
    1, 5, 10, 30};
static const char *g_screensaver_delay_map[7] = {};
static const char *g_night_enabled_map[3] = {};
static const char *g_night_screen_map[3] = {};
static const char *g_chime_mode_map[6] = {};
static const char *g_chime_volume_map[] = {
    "25%", "50%", "\n", "75%", "100%", ""};
static const char *g_chime_quiet_map[3] = {};
static const char *g_wifi_enabled_map[3] = {};

void setup_codec();
void setup_lvgl_display();
void setup_lvgl_input();
void lvgl_fs_init_littlefs();
void minivmac();
static void run_emulator();
static void update_diagnostics_ui();
static void update_wifi_options_ui();
static void refresh_language_ui();
static void apply_clock_face_theme();

void request_state(int state)
{
    g_requested_state = state;
}

void request_normal_state()
{
    request_state(UI_STATE_NORMAL);
}

void request_timer_state()
{
    request_state(UI_STATE_TIMER_EDITOR);
}

DateTime rtc_now()
{
    switch (g_rtc_type)
    {
    case RTC_TYPE_DS1307:
        return rtc_ds1307.now();
    case RTC_TYPE_DS3231:
        return rtc_ds3231.now();
    default:
        return DateTime(2000, 1, 1, 0, 0, 0);
    }
}

void rtc_adjust_datetime(const DateTime &date_time)
{
    switch (g_rtc_type)
    {
    case RTC_TYPE_DS1307:
        rtc_ds1307.adjust(date_time);
        break;
    case RTC_TYPE_DS3231:
        rtc_ds3231.adjust(date_time);
        break;
    default:
        break;
    }
}

static void delete_mp3_locked()
{
    if (mp3)
    {
        if (mp3->isRunning())
            mp3->stop();
        delete mp3;
        mp3 = nullptr;
    }
    if (file)
    {
        delete file;
        file = nullptr;
    }
}

static void stop_mp3_playback()
{
    if (g_mp3_lock)
        xSemaphoreTake(g_mp3_lock, portMAX_DELAY);
    delete_mp3_locked();
    portENTER_CRITICAL(&g_mp3_mux);
    g_mp3_finished = false;
    portEXIT_CRITICAL(&g_mp3_mux);
    if (g_mp3_lock)
        xSemaphoreGive(g_mp3_lock);
}

static bool start_mp3_playback(const char *path, uint8_t volume)
{
    if (!path || !audio_out)
        return false;

    if (g_mp3_lock)
        xSemaphoreTake(g_mp3_lock, portMAX_DELAY);
    delete_mp3_locked();
    if (es8311_handle)
        es8311_voice_volume_set(es8311_handle, volume, nullptr);
    audio_out->SetGain(1.0f);

    file = new AudioFileSourceLittleFS(path);
    mp3 = new AudioGeneratorMP3();
    const bool started = file && mp3 && mp3->begin(file, audio_out);
    if (!started)
        delete_mp3_locked();

    portENTER_CRITICAL(&g_mp3_mux);
    g_mp3_finished = false;
    portEXIT_CRITICAL(&g_mp3_mux);
    if (g_mp3_lock)
        xSemaphoreGive(g_mp3_lock);
    return started;
}

static bool consume_mp3_finished()
{
    bool finished = false;
    portENTER_CRITICAL(&g_mp3_mux);
    finished = g_mp3_finished;
    g_mp3_finished = false;
    portEXIT_CRITICAL(&g_mp3_mux);
    return finished;
}

static bool mp3_playback_running()
{
    if (g_mp3_lock)
        xSemaphoreTake(g_mp3_lock, portMAX_DELAY);
    const bool running = mp3 && mp3->isRunning();
    if (g_mp3_lock)
        xSemaphoreGive(g_mp3_lock);
    return running;
}

void alarm_snooze_current()
{
    if (g_active_alarm_index < 0)
        return;

    stop_mp3_playback();
    alarms_snooze((size_t)g_active_alarm_index, rtc_now());
    g_active_alarm_index = -1;
    request_normal_state();
}

void alarm_dismiss_current()
{
    if (g_active_alarm_index < 0)
        return;

    stop_mp3_playback();
    alarms_dismiss();
    g_active_alarm_index = -1;
    request_normal_state();
}

void timer_dismiss_current()
{
    stop_mp3_playback();
    request_normal_state();
}

static bool format_rtc_health(char *text, size_t text_size)
{
    if (g_rtc_type == RTC_TYPE_NONE)
    {
        snprintf(text, text_size, "%s", tr("RTC: not detected"));
        return false;
    }

    if (g_rtc_type == RTC_TYPE_DS1307 && !rtc_ds1307.isrunning())
    {
        snprintf(text, text_size, "%s",
                 tr("RTC: DS1307 stopped - check battery"));
        return false;
    }
    if (g_rtc_type == RTC_TYPE_DS3231 && rtc_ds3231.lostPower())
    {
        snprintf(text, text_size, "%s",
                 tr("RTC: DS3231 lost power - check battery"));
        return false;
    }

    const DateTime current = rtc_now();
    if (!current.isValid())
    {
        snprintf(text, text_size, "%s", tr("RTC: invalid date"));
        return false;
    }
    if (current.year() < 2024)
    {
        snprintf(text, text_size, tr("RTC: date not set (%04d)"),
                 current.year());
        return false;
    }

    snprintf(text, text_size, tr("RTC: %s OK"),
             g_rtc_type == RTC_TYPE_DS1307 ? "DS1307" : "DS3231");
    return true;
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
    if (g_calib_ui.cross &&
        !lv_obj_has_flag(g_calib_ui.cross, LV_OBJ_FLAG_HIDDEN))
        return;
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

static bool calib_axis_bounds(uint16_t near_a, uint16_t near_b,
                              uint16_t far_a, uint16_t far_b,
                              int near_target, int far_target,
                              int screen_size,
                              uint16_t &edge_min, uint16_t &edge_max)
{
    const int32_t near_raw = ((int32_t)near_a + near_b + 1) / 2;
    const int32_t far_raw = ((int32_t)far_a + far_b + 1) / 2;
    const int32_t target_span = far_target - near_target;
    const int32_t raw_span = far_raw - near_raw;
    if (target_span <= 0 || raw_span <= 0 || screen_size <= 1)
        return false;

    int32_t min_raw =
        near_raw - (raw_span * near_target + target_span / 2) / target_span;
    int32_t max_raw =
        far_raw +
        (raw_span * (screen_size - 1 - far_target) + target_span / 2) /
            target_span;

    min_raw = constrain(min_raw, 0, UINT16_MAX);
    max_raw = constrain(max_raw, 0, UINT16_MAX);
    if (min_raw >= max_raw)
        return false;

    edge_min = (uint16_t)min_raw;
    edge_max = (uint16_t)max_raw;
    return true;
}

static void apply_boot_brightness(BootBrightness choice, bool save_choice)
{
    g_boot_brightness = choice;
    if (save_choice)
        preferences.putUChar("boot_brightness", (uint8_t)choice);

    int brightness = preferences.getUChar("brightness", 6);
    if (brightness > kBrightnessMax)
        brightness = 6;
    if (choice == BOOT_BRIGHTNESS_LOWEST)
        brightness = 1;
    else if (choice == BOOT_BRIGHTNESS_HIGHEST)
        brightness = kBrightnessMax;

    encoder.setCount(brightness);
    g_last_saved_encoder = brightness;
    analogWrite(TFT_BL_VAR, brightness_to_pwm(brightness));
}

static uint8_t adjusted_hour(uint8_t hour, int delta)
{
    return (uint8_t)((hour + delta + 24) % 24);
}

static bool time_in_interval(uint16_t current,
                             uint16_t start,
                             uint16_t end)
{
    if (start == end)
        return false;
    if (start < end)
        return current >= start && current < end;
    return current >= start || current < end;
}

static NightDisplayState night_display_state(const DateTime &current)
{
    if (!g_night_mode.enabled ||
        g_rtc_type == RTC_TYPE_NONE ||
        current.year() < 2024)
    {
        return NIGHT_DISPLAY_NORMAL;
    }

    const uint16_t current_minutes =
        (uint16_t)current.hour() * 60 + current.minute();
    const uint16_t start_minutes =
        (uint16_t)g_night_mode.start_hour * 60;
    const uint16_t end_minutes =
        (uint16_t)g_night_mode.end_hour * 60;
    if (!time_in_interval(current_minutes, start_minutes, end_minutes))
        return NIGHT_DISPLAY_NORMAL;

    if (g_night_mode.screen_off_enabled)
    {
        const uint16_t off_minutes =
            (uint16_t)g_night_mode.screen_off_hour * 60;
        const uint16_t night_duration =
            (end_minutes + 1440 - start_minutes) % 1440;
        const uint16_t off_offset =
            (off_minutes + 1440 - start_minutes) % 1440;
        const uint16_t current_offset =
            (current_minutes + 1440 - start_minutes) % 1440;
        if (off_offset < night_duration && current_offset >= off_offset)
            return NIGHT_DISPLAY_OFF;
    }

    return NIGHT_DISPLAY_DIMMED;
}

static bool chime_quiet_now(const DateTime &current)
{
    if (!g_chime.quiet_enabled)
        return false;
    const uint16_t current_minutes =
        (uint16_t)current.hour() * 60 + current.minute();
    return time_in_interval(
        current_minutes,
        (uint16_t)g_chime.quiet_start_hour * 60,
        (uint16_t)g_chime.quiet_end_hour * 60);
}

static void maybe_start_chime(const DateTime &current)
{
    static uint32_t last_chime_minute = UINT32_MAX;
    if (g_chime.mode == CHIME_MODE_OFF ||
        g_rtc_type == RTC_TYPE_NONE ||
        current.year() < 2024)
    {
        return;
    }

    const uint8_t minute = current.minute();
    const bool due =
        minute == 0 ||
        (g_chime.mode == CHIME_MODE_QUARTER_HOUR &&
         (minute % 15) == 0);
    if (!due || current.second() > 4)
        return;

    const uint32_t minute_key = current.unixtime() / 60;
    if (minute_key == last_chime_minute)
        return;
    last_chime_minute = minute_key;

    if (chime_quiet_now(current) || mp3_playback_running())
        return;

    const size_t volume_count =
        sizeof(g_chime_volumes) / sizeof(g_chime_volumes[0]);
    if (g_chime.volume >= volume_count)
        return;
    start_mp3_playback(
        sound_selector_resolve_path(
            g_chime_sound_path, "/quack.mp3"),
        g_chime_volumes[g_chime.volume]);
}

static void set_checked_button(lv_obj_t *matrix, uint32_t selected)
{
    if (!matrix)
        return;
    lv_buttonmatrix_clear_button_ctrl_all(
        matrix, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_button_ctrl(
        matrix, selected, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_selected_button(matrix, selected);
}

static void update_language_selection(bool scroll_to_selected)
{
    const uint32_t selected =
        (uint32_t)localization_get_language();
    for (uint32_t i = 0; i < UI_LANGUAGE_COUNT; ++i)
    {
        lv_obj_t *item = g_boot_options_ui.language_items[i];
        if (!item)
            continue;
        if (i == selected)
            lv_obj_add_state(item, LV_STATE_CHECKED);
        else
            lv_obj_remove_state(item, LV_STATE_CHECKED);
    }

    if (scroll_to_selected &&
        selected < UI_LANGUAGE_COUNT &&
        g_boot_options_ui.language_items[selected])
    {
        lv_obj_scroll_to_view(
            g_boot_options_ui.language_items[selected],
            LV_ANIM_OFF);
    }
}

static void update_menu_titles()
{
    if (!g_ui.menu_titles)
        return;

    char titles[96];
    snprintf(
        titles, sizeof(titles), "%s  %s  %s  %s",
        tr("File"), tr("Edit"), tr("View"), tr("Special"));
    lv_label_set_text(g_ui.menu_titles, titles);
}

static void update_boot_message()
{
    if (g_ui.boot_message)
        lv_label_set_text(
            g_ui.boot_message, tr("Welcome to Macintosh."));
}

static void update_boot_translation_maps()
{
    g_brightness_map[0] = tr("Latest");
    g_brightness_map[1] = tr("Lowest");
    g_brightness_map[2] = tr("Highest");
    g_brightness_map[3] = "";
    g_remember_map[0] = tr("One time");
    g_remember_map[1] = tr("Remember");
    g_remember_map[2] = "";
    g_clock_face_map[0] = tr("Macintosh");
    g_clock_face_map[1] = tr("Compact");
    g_clock_face_map[2] = "\n";
    g_clock_face_map[3] = tr("Analog");
    g_clock_face_map[4] = tr("Flip");
    g_clock_face_map[5] = "";
    g_clock_theme_map[0] = tr("Light");
    g_clock_theme_map[1] = tr("Dark");
    g_clock_theme_map[2] = "";
    g_screensaver_map[0] = tr("Off");
    g_screensaver_map[1] = tr("After Dark");
    g_screensaver_map[2] = "";
    g_screensaver_delay_map[0] = tr("1 min");
    g_screensaver_delay_map[1] = tr("5 min");
    g_screensaver_delay_map[2] = "\n";
    g_screensaver_delay_map[3] = tr("10 min");
    g_screensaver_delay_map[4] = tr("30 min");
    g_screensaver_delay_map[5] = "";
    g_night_enabled_map[0] = tr("Disabled");
    g_night_enabled_map[1] = tr("Enabled");
    g_night_enabled_map[2] = "";
    g_night_screen_map[0] = tr("Dim only");
    g_night_screen_map[1] = tr("Screen off");
    g_night_screen_map[2] = "";
    g_chime_mode_map[0] = tr("Off");
    g_chime_mode_map[1] = "\n";
    g_chime_mode_map[2] = tr("Hourly");
    g_chime_mode_map[3] = "\n";
    g_chime_mode_map[4] = tr("Quarter hour");
    g_chime_mode_map[5] = "";
    g_chime_quiet_map[0] = tr("Disabled");
    g_chime_quiet_map[1] = tr("Enabled");
    g_chime_quiet_map[2] = "";
    g_wifi_enabled_map[0] = tr("Off");
    g_wifi_enabled_map[1] = tr("On");
    g_wifi_enabled_map[2] = "";
}

static void update_night_options_ui()
{
    snprintf(g_night_start_text, sizeof(g_night_start_text),
             "%02u:00", (unsigned)g_night_mode.start_hour);
    snprintf(g_night_end_text, sizeof(g_night_end_text),
             "%02u:00", (unsigned)g_night_mode.end_hour);
    snprintf(g_night_off_text, sizeof(g_night_off_text),
             "%02u:00", (unsigned)g_night_mode.screen_off_hour);

    if (g_boot_options_ui.night_start_options)
    {
        lv_buttonmatrix_set_map(
            g_boot_options_ui.night_start_options, g_night_start_map);
        lv_buttonmatrix_set_map(
            g_boot_options_ui.night_end_options, g_night_end_map);
        lv_buttonmatrix_set_map(
            g_boot_options_ui.night_off_options, g_night_off_map);
        set_checked_button(
            g_boot_options_ui.night_enabled_options,
            g_night_mode.enabled ? 1 : 0);
        set_checked_button(
            g_boot_options_ui.night_screen_options,
            g_night_mode.screen_off_enabled ? 1 : 0);
    }
}

static void update_chime_options_ui()
{
    snprintf(
        g_chime_quiet_start_text,
        sizeof(g_chime_quiet_start_text),
        "%02u:00", (unsigned)g_chime.quiet_start_hour);
    snprintf(
        g_chime_quiet_end_text,
        sizeof(g_chime_quiet_end_text),
        "%02u:00", (unsigned)g_chime.quiet_end_hour);

    if (!g_boot_options_ui.chime_mode_options)
        return;
    lv_buttonmatrix_set_map(
        g_boot_options_ui.chime_quiet_start_options,
        g_chime_quiet_start_map);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.chime_quiet_end_options,
        g_chime_quiet_end_map);
    set_checked_button(
        g_boot_options_ui.chime_mode_options, (uint32_t)g_chime.mode);
    sound_selector_set_path(
        &g_boot_options_ui.chime_sound_selector,
        g_chime_sound_path);
    sound_selector_set_preview_volume(
        &g_boot_options_ui.chime_sound_selector,
        g_chime_volumes[g_chime.volume]);
    set_checked_button(
        g_boot_options_ui.chime_volume_options, g_chime.volume);
    set_checked_button(
        g_boot_options_ui.chime_quiet_options,
        g_chime.quiet_enabled ? 1 : 0);
}

static void update_wifi_options_ui()
{
    if (!g_boot_options_ui.wifi_enabled_options)
        return;

    const WifiModeSnapshot wifi = wifi_mode_snapshot();
    set_checked_button(
        g_boot_options_ui.wifi_enabled_options,
        wifi.enabled ? 1 : 0);

    char status[144];
    if (!wifi.enabled)
    {
        snprintf(status, sizeof(status), "%s",
                 tr("Wi-Fi disabled\nClock remains fully offline"));
    }
    else if (!wifi.configured)
    {
        snprintf(status, sizeof(status), "%s",
                 tr("Setup required\nChoose Setup Wi-Fi below"));
    }
    else if (wifi.connected)
    {
        snprintf(status, sizeof(status), tr("Online: %s\n%s"),
                 wifi.location[0] ? wifi.location : wifi.city,
                 wifi.timezone[0]
                     ? wifi.timezone
                     : tr(wifi.status));
    }
    else
    {
        snprintf(status, sizeof(status), "%s\n%s",
                 wifi.ssid, tr(wifi.status));
    }
    lv_label_set_text(g_boot_options_ui.wifi_status, status);
}

static void language_event(lv_event_t *event)
{
    lv_obj_t *item = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        (uint32_t)(uintptr_t)lv_obj_get_user_data(item);
    if (selected >= UI_LANGUAGE_COUNT)
        return;
    localization_set_language((UiLanguage)selected);
    preferences.putUChar("language", (uint8_t)selected);
    refresh_language_ui();
}

static void date_format_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= UI_DATE_FORMAT_COUNT)
        return;
    g_date_format = (UiDateFormat)selected;
    preferences.putUChar("date_format", (uint8_t)g_date_format);
    datetime_ui_set_date_format(g_date_format);
}

static void temperature_unit_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= UI_TEMPERATURE_UNIT_COUNT)
        return;
    g_temperature_unit = (UiTemperatureUnit)selected;
    preferences.putUChar(
        "temp_unit", (uint8_t)g_temperature_unit);
}

static void clock_face_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= CLOCK_FACE_COUNT)
        return;
    g_clock_face = (ClockFace)selected;
    preferences.putUChar("clock_face", (uint8_t)g_clock_face);
}

static void clock_theme_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= CLOCK_THEME_COUNT)
        return;
    g_clock_theme = (ClockTheme)selected;
    preferences.putUChar(
        "clock_theme", (uint8_t)g_clock_theme);
    if (g_clock_faces_ui.compact)
        apply_clock_face_theme();
}

static void screensaver_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= SCREENSAVER_MODE_COUNT)
        return;
    g_screensaver_mode = (ScreensaverMode)selected;
    preferences.putUChar(
        "screen_mode", (uint8_t)g_screensaver_mode);
}

static void screensaver_delay_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >=
        sizeof(g_screensaver_delays_minutes) /
            sizeof(g_screensaver_delays_minutes[0]))
    {
        return;
    }
    g_screensaver_delay_index = (uint8_t)selected;
    preferences.putUChar(
        "screen_delay", g_screensaver_delay_index);
}

static void chime_mode_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= CHIME_MODE_COUNT)
        return;
    g_chime.mode = (ChimeMode)selected;
    preferences.putUChar("chime_mode", (uint8_t)g_chime.mode);
}

static void chime_sound_changed(
    const char *path, void *user_data)
{
    (void)user_data;
    if (!path)
        return;
    strlcpy(
        g_chime_sound_path, path,
        sizeof(g_chime_sound_path));
    preferences.putString("chime_path", g_chime_sound_path);
}

static void chime_volume_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >=
        sizeof(g_chime_volumes) / sizeof(g_chime_volumes[0]))
    {
        return;
    }
    g_chime.volume = (uint8_t)selected;
    preferences.putUChar("chime_volume", g_chime.volume);
    sound_selector_set_preview_volume(
        &g_boot_options_ui.chime_sound_selector,
        g_chime_volumes[g_chime.volume]);
}

static void chime_quiet_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= 2)
        return;
    g_chime.quiet_enabled = selected == 1;
    preferences.putBool("chime_quiet", g_chime.quiet_enabled);
}

static void chime_quiet_start_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected != 0 && selected != 2)
        return;
    g_chime.quiet_start_hour = adjusted_hour(
        g_chime.quiet_start_hour, selected == 0 ? -1 : 1);
    preferences.putUChar(
        "quiet_start", g_chime.quiet_start_hour);
    update_chime_options_ui();
}

static void chime_quiet_end_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected != 0 && selected != 2)
        return;
    g_chime.quiet_end_hour = adjusted_hour(
        g_chime.quiet_end_hour, selected == 0 ? -1 : 1);
    preferences.putUChar(
        "quiet_end", g_chime.quiet_end_hour);
    update_chime_options_ui();
}

static void night_enabled_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= 2)
        return;
    g_night_mode.enabled = selected == 1;
    preferences.putBool("night_enabled", g_night_mode.enabled);
}

static void night_screen_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= 2)
        return;
    g_night_mode.screen_off_enabled = selected == 1;
    preferences.putBool("night_off", g_night_mode.screen_off_enabled);
}

static void night_start_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected != 0 && selected != 2)
        return;
    g_night_mode.start_hour =
        adjusted_hour(g_night_mode.start_hour, selected == 0 ? -1 : 1);
    preferences.putUChar("night_start", g_night_mode.start_hour);
    update_night_options_ui();
}

static void night_end_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected != 0 && selected != 2)
        return;
    g_night_mode.end_hour =
        adjusted_hour(g_night_mode.end_hour, selected == 0 ? -1 : 1);
    preferences.putUChar("night_end", g_night_mode.end_hour);
    update_night_options_ui();
}

static void night_off_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected != 0 && selected != 2)
        return;
    g_night_mode.screen_off_hour =
        adjusted_hour(
            g_night_mode.screen_off_hour, selected == 0 ? -1 : 1);
    preferences.putUChar(
        "night_off_at", g_night_mode.screen_off_hour);
    update_night_options_ui();
}

static void wifi_enabled_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected =
        lv_buttonmatrix_get_selected_button(options);
    if (selected >= 2)
        return;
    wifi_mode_set_enabled(selected == 1);
    update_wifi_options_ui();
}

static void boot_brightness_event(lv_event_t *event)
{
    lv_obj_t *options = (lv_obj_t *)lv_event_get_target(event);
    const uint32_t selected = lv_buttonmatrix_get_selected_button(options);
    if (selected <= BOOT_BRIGHTNESS_HIGHEST)
        apply_boot_brightness((BootBrightness)selected, true);
}

static bool boot_remember_selection()
{
    return g_boot_options_ui.remember_selection &&
           lv_buttonmatrix_has_button_ctrl(
               g_boot_options_ui.remember_selection,
               1, LV_BUTTONMATRIX_CTRL_CHECKED);
}

static void remember_boot_mode(bool emulator)
{
    if (!boot_remember_selection())
        return;
    g_boot_floppy_emulator = emulator;
    preferences.putBool("floppy_emulator", emulator);
}

static void boot_start_clock_event(lv_event_t *event)
{
    (void)event;
    remember_boot_mode(false);
    request_state(UI_STATE_EMPTY_SCREEN);
}

static void boot_start_emulator_event(lv_event_t *event)
{
    (void)event;
    remember_boot_mode(true);
    request_state(UI_STATE_EMULATOR);
}

static void boot_diagnostics_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_DIAGNOSTICS);
}

static void boot_wifi_setup_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_WIFI_SETUP);
}

static void wifi_setup_back_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_BOOT_OPTIONS);
}

static void boot_exit_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_NORMAL);
}

static void diagnostics_back_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_BOOT_OPTIONS);
}

static void set_boot_options_page(BootOptionsPage page)
{
    if (page >= BOOT_OPTIONS_PAGE_COUNT)
        return;

    const char *page_names[BOOT_OPTIONS_PAGE_COUNT] = {
        tr("Start"), tr("Preferences"), tr("Language"),
        tr("Regional"), tr("Clock Face"), tr("Clock Theme"),
        tr("Screensaver"),
        tr("Night Schedule"), tr("Night Screen"), tr("Chime"),
        tr("Chime Sound"), tr("Chime Volume"), tr("Quiet Hours"),
        tr("Wi-Fi"), tr("Tools")};
    g_boot_options_page = page;
    for (size_t i = 0; i < BOOT_OPTIONS_PAGE_COUNT; ++i)
        lv_obj_add_flag(
            g_boot_options_ui.pages[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(
        g_boot_options_ui.pages[page], LV_OBJ_FLAG_HIDDEN);

    char title[72];
    snprintf(title, sizeof(title), "%s - %s (%u/%u)",
             tr("Boot Options"), page_names[page],
             (unsigned)page + 1,
             (unsigned)BOOT_OPTIONS_PAGE_COUNT);
    lv_label_set_text(g_boot_options_ui.title, title);

    if (page == BOOT_OPTIONS_START)
        lv_obj_add_flag(
            g_boot_options_ui.previous, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(
            g_boot_options_ui.previous, LV_OBJ_FLAG_HIDDEN);

    if (page == BOOT_OPTIONS_TOOLS)
        lv_obj_add_flag(
            g_boot_options_ui.next, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(
            g_boot_options_ui.next, LV_OBJ_FLAG_HIDDEN);
}

static void boot_options_previous_event(lv_event_t *event)
{
    (void)event;
    if (g_boot_options_page > BOOT_OPTIONS_START)
    {
        set_boot_options_page(
            (BootOptionsPage)(g_boot_options_page - 1));
    }
}

static void boot_options_next_event(lv_event_t *event)
{
    (void)event;
    if (g_boot_options_page < BOOT_OPTIONS_TOOLS)
    {
        set_boot_options_page(
            (BootOptionsPage)(g_boot_options_page + 1));
    }
}

static void boot_options_continue_visual_event(lv_event_t *event)
{
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(event);
    const lv_event_code_t code = lv_event_get_code(event);
    lv_obj_set_style_text_color(
        label,
        code == LV_EVENT_PRESSED ? lv_color_white() : lv_color_black(),
        0);
}

static void style_boot_options_matrix(lv_obj_t *matrix)
{
    const lv_style_selector_t checked_items =
        (lv_style_selector_t)LV_PART_ITEMS |
        (lv_style_selector_t)LV_STATE_CHECKED;
    const lv_style_selector_t pressed_items =
        (lv_style_selector_t)LV_PART_ITEMS |
        (lv_style_selector_t)LV_STATE_PRESSED;

    lv_obj_set_style_bg_opa(matrix, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(matrix, 0, 0);
    lv_obj_set_style_radius(matrix, 0, 0);
    lv_obj_set_style_pad_all(matrix, 0, 0);
    lv_obj_set_style_pad_row(matrix, 10, 0);
    lv_obj_set_style_pad_column(matrix, 10, 0);
    lv_obj_set_style_text_font(matrix, &lv_font_chicago_8, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_border_color(matrix, lv_color_black(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(matrix, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(matrix, 4, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(matrix, 0, LV_PART_ITEMS);
    lv_obj_set_style_outline_width(matrix, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, lv_color_black(), checked_items);
    lv_obj_set_style_text_color(matrix, lv_color_white(), checked_items);
    lv_obj_set_style_bg_color(matrix, lv_color_black(), pressed_items);
    lv_obj_set_style_text_color(matrix, lv_color_white(), pressed_items);
}

static lv_obj_t *create_action_button(lv_obj_t *parent,
                                      const char *text,
                                      lv_event_cb_t callback)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_style_bg_color(button, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(button, lv_color_black(), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_radius(button, 4, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_outline_width(button, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_black(), LV_STATE_PRESSED);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(button, boot_options_continue_visual_event,
                        LV_EVENT_PRESSED, label);
    lv_obj_add_event_cb(button, boot_options_continue_visual_event,
                        LV_EVENT_RELEASED, label);
    lv_obj_add_event_cb(button, boot_options_continue_visual_event,
                        LV_EVENT_PRESS_LOST, label);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
    return button;
}

static lv_obj_t *create_boot_options_page(lv_obj_t *parent)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, 276, 130);
    lv_obj_align(page, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    return page;
}

static void init_boot_options_ui(lv_obj_t *screen)
{
    update_boot_translation_maps();

    g_boot_options_ui.panel = lv_obj_create(screen);
    lv_obj_set_size(g_boot_options_ui.panel, 292, 208);
    lv_obj_center(g_boot_options_ui.panel);
    lv_obj_set_style_bg_color(g_boot_options_ui.panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_boot_options_ui.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_boot_options_ui.panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_boot_options_ui.panel, 2, 0);
    lv_obj_set_style_radius(g_boot_options_ui.panel, 0, 0);
    lv_obj_set_style_pad_all(g_boot_options_ui.panel, 6, 0);

    g_boot_options_ui.title =
        lv_label_create(g_boot_options_ui.panel);
    lv_label_set_text(g_boot_options_ui.title, tr("Boot Options"));
    lv_obj_set_style_text_font(
        g_boot_options_ui.title, &lv_font_chicago_8, 0);
    lv_obj_align(
        g_boot_options_ui.title, LV_ALIGN_TOP_MID, 0, 0);

    for (size_t i = 0; i < BOOT_OPTIONS_PAGE_COUNT; ++i)
    {
        g_boot_options_ui.pages[i] =
            create_boot_options_page(g_boot_options_ui.panel);
    }

    lv_obj_t *preferences_page =
        g_boot_options_ui.pages[BOOT_OPTIONS_PREFERENCES];
    g_boot_options_ui.brightness_label = lv_label_create(preferences_page);
    lv_label_set_text(g_boot_options_ui.brightness_label, tr("Brightness"));
    lv_obj_set_style_text_font(g_boot_options_ui.brightness_label, &lv_font_chicago_8, 0);
    lv_obj_align(g_boot_options_ui.brightness_label, LV_ALIGN_TOP_MID, 0, 0);

    g_boot_options_ui.brightness_options =
        lv_buttonmatrix_create(preferences_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.brightness_options, g_brightness_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.brightness_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.brightness_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        g_boot_options_ui.brightness_options, true);
    lv_obj_set_size(
        g_boot_options_ui.brightness_options, 260, 48);
    lv_obj_align(
        g_boot_options_ui.brightness_options,
        LV_ALIGN_TOP_MID, 0, 13);
    style_boot_options_matrix(g_boot_options_ui.brightness_options);
    lv_obj_add_event_cb(
        g_boot_options_ui.brightness_options, boot_brightness_event,
                        LV_EVENT_VALUE_CHANGED, nullptr);

    g_boot_options_ui.remember_label = lv_label_create(preferences_page);
    lv_label_set_text(g_boot_options_ui.remember_label, tr("Default boot mode"));
    lv_obj_set_style_text_font(g_boot_options_ui.remember_label, &lv_font_chicago_8, 0);
    lv_obj_align(g_boot_options_ui.remember_label, LV_ALIGN_TOP_MID, 0, 67);

    g_boot_options_ui.remember_selection =
        lv_buttonmatrix_create(preferences_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.remember_selection, g_remember_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.remember_selection,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.remember_selection,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        g_boot_options_ui.remember_selection, true);
    lv_obj_set_size(
        g_boot_options_ui.remember_selection, 260, 48);
    lv_obj_align(
        g_boot_options_ui.remember_selection,
        LV_ALIGN_BOTTOM_MID, 0, 0);
    style_boot_options_matrix(
        g_boot_options_ui.remember_selection);

    lv_obj_t *language_page =
        g_boot_options_ui.pages[BOOT_OPTIONS_LANGUAGE];
    g_boot_options_ui.language_options =
        lv_list_create(language_page);
    lv_obj_set_size(
        g_boot_options_ui.language_options, 260, 124);
    lv_obj_center(g_boot_options_ui.language_options);
    selector_list_style_container(
        g_boot_options_ui.language_options);
    for (uint32_t i = 0; i < UI_LANGUAGE_COUNT; ++i)
    {
        lv_obj_t *item = lv_list_add_button(
            g_boot_options_ui.language_options, nullptr,
            localization_language_name((UiLanguage)i));
        g_boot_options_ui.language_items[i] = item;
        selector_list_style_item(item);
        lv_obj_set_user_data(item, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(
            item, language_event, LV_EVENT_CLICKED, nullptr);
    }

    lv_obj_t *regional_page =
        g_boot_options_ui.pages[BOOT_OPTIONS_REGIONAL];
    g_boot_options_ui.date_format_label =
        lv_label_create(regional_page);
    lv_label_set_text(
        g_boot_options_ui.date_format_label, tr("Date format"));
    lv_obj_set_style_text_font(
        g_boot_options_ui.date_format_label,
        &lv_font_chicago_8, 0);
    lv_obj_align(
        g_boot_options_ui.date_format_label,
        LV_ALIGN_TOP_MID, 0, 0);

    g_boot_options_ui.date_format_options =
        lv_buttonmatrix_create(regional_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.date_format_options,
        g_date_format_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.date_format_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.date_format_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        g_boot_options_ui.date_format_options, true);
    lv_obj_set_size(
        g_boot_options_ui.date_format_options, 260, 42);
    lv_obj_align(
        g_boot_options_ui.date_format_options,
        LV_ALIGN_TOP_MID, 0, 16);
    style_boot_options_matrix(
        g_boot_options_ui.date_format_options);
    lv_obj_set_style_pad_column(
        g_boot_options_ui.date_format_options, 6, 0);
    lv_obj_add_event_cb(
        g_boot_options_ui.date_format_options,
        date_format_event, LV_EVENT_VALUE_CHANGED, nullptr);

    g_boot_options_ui.temperature_unit_label =
        lv_label_create(regional_page);
    lv_label_set_text(
        g_boot_options_ui.temperature_unit_label,
        tr("Temperature unit"));
    lv_obj_set_style_text_font(
        g_boot_options_ui.temperature_unit_label,
        &lv_font_chicago_8, 0);
    lv_obj_align(
        g_boot_options_ui.temperature_unit_label,
        LV_ALIGN_TOP_MID, 0, 69);

    g_boot_options_ui.temperature_unit_options =
        lv_buttonmatrix_create(regional_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.temperature_unit_options,
        g_temperature_unit_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.temperature_unit_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.temperature_unit_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        g_boot_options_ui.temperature_unit_options, true);
    lv_obj_set_size(
        g_boot_options_ui.temperature_unit_options, 260, 42);
    lv_obj_align(
        g_boot_options_ui.temperature_unit_options,
        LV_ALIGN_BOTTOM_MID, 0, 0);
    style_boot_options_matrix(
        g_boot_options_ui.temperature_unit_options);
    lv_obj_set_style_pad_column(
        g_boot_options_ui.temperature_unit_options, 10, 0);
    lv_obj_add_event_cb(
        g_boot_options_ui.temperature_unit_options,
        temperature_unit_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *clock_face_page =
        g_boot_options_ui.pages[BOOT_OPTIONS_CLOCK_FACE];
    g_boot_options_ui.clock_face_options =
        lv_buttonmatrix_create(clock_face_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.clock_face_options,
        g_clock_face_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.clock_face_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.clock_face_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        g_boot_options_ui.clock_face_options, true);
    lv_obj_set_size(
        g_boot_options_ui.clock_face_options, 260, 124);
    lv_obj_center(g_boot_options_ui.clock_face_options);
    style_boot_options_matrix(
        g_boot_options_ui.clock_face_options);
    lv_obj_add_event_cb(
        g_boot_options_ui.clock_face_options,
        clock_face_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *clock_theme_page =
        g_boot_options_ui.pages[BOOT_OPTIONS_CLOCK_THEME];
    g_boot_options_ui.clock_theme_options =
        lv_buttonmatrix_create(clock_theme_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.clock_theme_options,
        g_clock_theme_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.clock_theme_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.clock_theme_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        g_boot_options_ui.clock_theme_options, true);
    lv_obj_set_size(
        g_boot_options_ui.clock_theme_options, 260, 124);
    lv_obj_center(g_boot_options_ui.clock_theme_options);
    style_boot_options_matrix(
        g_boot_options_ui.clock_theme_options);
    lv_obj_add_event_cb(
        g_boot_options_ui.clock_theme_options,
        clock_theme_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *screensaver_page =
        g_boot_options_ui.pages[BOOT_OPTIONS_SCREENSAVER];
    g_boot_options_ui.screensaver_options =
        lv_buttonmatrix_create(screensaver_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.screensaver_options,
        g_screensaver_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.screensaver_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.screensaver_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        g_boot_options_ui.screensaver_options, true);
    lv_obj_set_size(
        g_boot_options_ui.screensaver_options, 260, 38);
    lv_obj_align(
        g_boot_options_ui.screensaver_options,
        LV_ALIGN_TOP_MID, 0, 0);
    style_boot_options_matrix(
        g_boot_options_ui.screensaver_options);
    lv_obj_add_event_cb(
        g_boot_options_ui.screensaver_options,
        screensaver_event, LV_EVENT_VALUE_CHANGED, nullptr);

    g_boot_options_ui.screensaver_delay_label =
        lv_label_create(screensaver_page);
    lv_label_set_text(
        g_boot_options_ui.screensaver_delay_label,
        tr("Start after"));
    lv_obj_set_style_text_font(
        g_boot_options_ui.screensaver_delay_label,
        &lv_font_chicago_8, 0);
    lv_obj_align(
        g_boot_options_ui.screensaver_delay_label,
        LV_ALIGN_TOP_MID, 0, 44);

    g_boot_options_ui.screensaver_delay_options =
        lv_buttonmatrix_create(screensaver_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.screensaver_delay_options,
        g_screensaver_delay_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.screensaver_delay_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.screensaver_delay_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        g_boot_options_ui.screensaver_delay_options, true);
    lv_obj_set_size(
        g_boot_options_ui.screensaver_delay_options, 260, 64);
    lv_obj_align(
        g_boot_options_ui.screensaver_delay_options,
        LV_ALIGN_BOTTOM_MID, 0, 0);
    style_boot_options_matrix(
        g_boot_options_ui.screensaver_delay_options);
    lv_obj_set_style_pad_row(
        g_boot_options_ui.screensaver_delay_options, 5, 0);
    lv_obj_add_event_cb(
        g_boot_options_ui.screensaver_delay_options,
        screensaver_delay_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *night_schedule_page =
        g_boot_options_ui.pages[BOOT_OPTIONS_NIGHT_SCHEDULE];
    g_boot_options_ui.night_enabled_options =
        lv_buttonmatrix_create(night_schedule_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.night_enabled_options, g_night_enabled_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.night_enabled_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.night_enabled_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        g_boot_options_ui.night_enabled_options, true);
    lv_obj_set_size(
        g_boot_options_ui.night_enabled_options, 260, 28);
    lv_obj_align(
        g_boot_options_ui.night_enabled_options,
        LV_ALIGN_TOP_MID, 0, 0);
    style_boot_options_matrix(
        g_boot_options_ui.night_enabled_options);
    lv_obj_add_event_cb(
        g_boot_options_ui.night_enabled_options,
        night_enabled_event, LV_EVENT_VALUE_CHANGED, nullptr);

    g_boot_options_ui.dim_from_label = lv_label_create(night_schedule_page);
    lv_label_set_text(g_boot_options_ui.dim_from_label, tr("Dim from"));
    lv_obj_set_style_text_font(g_boot_options_ui.dim_from_label, &lv_font_chicago_8, 0);
    lv_obj_align(g_boot_options_ui.dim_from_label, LV_ALIGN_TOP_MID, 0, 34);

    g_boot_options_ui.night_start_options =
        lv_buttonmatrix_create(night_schedule_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.night_start_options, g_night_start_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.night_start_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_obj_set_size(
        g_boot_options_ui.night_start_options, 260, 28);
    lv_obj_align(
        g_boot_options_ui.night_start_options,
        LV_ALIGN_TOP_MID, 0, 50);
    style_boot_options_matrix(
        g_boot_options_ui.night_start_options);
    lv_obj_set_style_pad_column(
        g_boot_options_ui.night_start_options, 18, 0);
    lv_obj_add_event_cb(
        g_boot_options_ui.night_start_options,
        night_start_event, LV_EVENT_VALUE_CHANGED, nullptr);

    g_boot_options_ui.normal_at_label = lv_label_create(night_schedule_page);
    lv_label_set_text(g_boot_options_ui.normal_at_label, tr("Normal at"));
    lv_obj_set_style_text_font(g_boot_options_ui.normal_at_label, &lv_font_chicago_8, 0);
    lv_obj_align(g_boot_options_ui.normal_at_label, LV_ALIGN_TOP_MID, 0, 84);

    g_boot_options_ui.night_end_options =
        lv_buttonmatrix_create(night_schedule_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.night_end_options, g_night_end_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.night_end_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_obj_set_size(
        g_boot_options_ui.night_end_options, 260, 28);
    lv_obj_align(
        g_boot_options_ui.night_end_options,
        LV_ALIGN_BOTTOM_MID, 0, 0);
    style_boot_options_matrix(
        g_boot_options_ui.night_end_options);
    lv_obj_set_style_pad_column(
        g_boot_options_ui.night_end_options, 18, 0);
    lv_obj_add_event_cb(
        g_boot_options_ui.night_end_options,
        night_end_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *night_screen_page =
        g_boot_options_ui.pages[BOOT_OPTIONS_NIGHT_SCREEN];
    g_boot_options_ui.night_screen_options =
        lv_buttonmatrix_create(night_screen_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.night_screen_options, g_night_screen_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.night_screen_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.night_screen_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        g_boot_options_ui.night_screen_options, true);
    lv_obj_set_size(
        g_boot_options_ui.night_screen_options, 260, 52);
    lv_obj_align(
        g_boot_options_ui.night_screen_options,
        LV_ALIGN_TOP_MID, 0, 0);
    style_boot_options_matrix(
        g_boot_options_ui.night_screen_options);
    lv_obj_add_event_cb(
        g_boot_options_ui.night_screen_options,
        night_screen_event, LV_EVENT_VALUE_CHANGED, nullptr);

    g_boot_options_ui.screen_off_label = lv_label_create(night_screen_page);
    lv_label_set_text(g_boot_options_ui.screen_off_label, tr("Screen off at"));
    lv_obj_set_style_text_font(g_boot_options_ui.screen_off_label, &lv_font_chicago_8, 0);
    lv_obj_align(g_boot_options_ui.screen_off_label, LV_ALIGN_TOP_MID, 0, 59);

    g_boot_options_ui.night_off_options =
        lv_buttonmatrix_create(night_screen_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.night_off_options, g_night_off_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.night_off_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_obj_set_size(
        g_boot_options_ui.night_off_options, 260, 50);
    lv_obj_align(
        g_boot_options_ui.night_off_options,
        LV_ALIGN_BOTTOM_MID, 0, 0);
    style_boot_options_matrix(
        g_boot_options_ui.night_off_options);
    lv_obj_add_event_cb(
        g_boot_options_ui.night_off_options,
        night_off_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *chime_page =
        g_boot_options_ui.pages[BOOT_OPTIONS_CHIME];
    g_boot_options_ui.chime_mode_options =
        lv_buttonmatrix_create(chime_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.chime_mode_options, g_chime_mode_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.chime_mode_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.chime_mode_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        g_boot_options_ui.chime_mode_options, true);
    lv_obj_set_size(
        g_boot_options_ui.chime_mode_options, 260, 124);
    lv_obj_center(g_boot_options_ui.chime_mode_options);
    style_boot_options_matrix(
        g_boot_options_ui.chime_mode_options);
    lv_obj_add_event_cb(
        g_boot_options_ui.chime_mode_options,
        chime_mode_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *chime_sound_page =
        g_boot_options_ui.pages[BOOT_OPTIONS_CHIME_SOUND];
    sound_selector_create(
        &g_boot_options_ui.chime_sound_selector,
        chime_sound_page,
        g_chime_sound_path,
        g_chime_volumes[g_chime.volume],
        chime_sound_changed,
        nullptr);

    lv_obj_t *chime_volume_page =
        g_boot_options_ui.pages[BOOT_OPTIONS_CHIME_VOLUME];
    g_boot_options_ui.chime_volume_options =
        lv_buttonmatrix_create(chime_volume_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.chime_volume_options, g_chime_volume_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.chime_volume_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.chime_volume_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        g_boot_options_ui.chime_volume_options, true);
    lv_obj_set_size(
        g_boot_options_ui.chime_volume_options, 260, 124);
    lv_obj_center(g_boot_options_ui.chime_volume_options);
    style_boot_options_matrix(
        g_boot_options_ui.chime_volume_options);
    lv_obj_add_event_cb(
        g_boot_options_ui.chime_volume_options,
        chime_volume_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *chime_quiet_page =
        g_boot_options_ui.pages[BOOT_OPTIONS_CHIME_QUIET];
    g_boot_options_ui.chime_quiet_options =
        lv_buttonmatrix_create(chime_quiet_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.chime_quiet_options, g_chime_quiet_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.chime_quiet_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.chime_quiet_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        g_boot_options_ui.chime_quiet_options, true);
    lv_obj_set_size(
        g_boot_options_ui.chime_quiet_options, 260, 28);
    lv_obj_align(
        g_boot_options_ui.chime_quiet_options,
        LV_ALIGN_TOP_MID, 0, 0);
    style_boot_options_matrix(
        g_boot_options_ui.chime_quiet_options);
    lv_obj_add_event_cb(
        g_boot_options_ui.chime_quiet_options,
        chime_quiet_event, LV_EVENT_VALUE_CHANGED, nullptr);

    g_boot_options_ui.quiet_from_label = lv_label_create(chime_quiet_page);
    lv_label_set_text(g_boot_options_ui.quiet_from_label, tr("Quiet from"));
    lv_obj_set_style_text_font(g_boot_options_ui.quiet_from_label, &lv_font_chicago_8, 0);
    lv_obj_align(g_boot_options_ui.quiet_from_label, LV_ALIGN_TOP_MID, 0, 34);

    g_boot_options_ui.chime_quiet_start_options =
        lv_buttonmatrix_create(chime_quiet_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.chime_quiet_start_options,
        g_chime_quiet_start_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.chime_quiet_start_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_obj_set_size(
        g_boot_options_ui.chime_quiet_start_options, 260, 28);
    lv_obj_align(
        g_boot_options_ui.chime_quiet_start_options,
        LV_ALIGN_TOP_MID, 0, 50);
    style_boot_options_matrix(
        g_boot_options_ui.chime_quiet_start_options);
    lv_obj_set_style_pad_column(
        g_boot_options_ui.chime_quiet_start_options, 18, 0);
    lv_obj_add_event_cb(
        g_boot_options_ui.chime_quiet_start_options,
        chime_quiet_start_event, LV_EVENT_VALUE_CHANGED, nullptr);

    g_boot_options_ui.quiet_end_label = lv_label_create(chime_quiet_page);
    lv_label_set_text(g_boot_options_ui.quiet_end_label, tr("Quiet ends"));
    lv_obj_set_style_text_font(g_boot_options_ui.quiet_end_label, &lv_font_chicago_8, 0);
    lv_obj_align(g_boot_options_ui.quiet_end_label, LV_ALIGN_TOP_MID, 0, 84);

    g_boot_options_ui.chime_quiet_end_options =
        lv_buttonmatrix_create(chime_quiet_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.chime_quiet_end_options,
        g_chime_quiet_end_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.chime_quiet_end_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_obj_set_size(
        g_boot_options_ui.chime_quiet_end_options, 260, 28);
    lv_obj_align(
        g_boot_options_ui.chime_quiet_end_options,
        LV_ALIGN_BOTTOM_MID, 0, 0);
    style_boot_options_matrix(
        g_boot_options_ui.chime_quiet_end_options);
    lv_obj_set_style_pad_column(
        g_boot_options_ui.chime_quiet_end_options, 18, 0);
    lv_obj_add_event_cb(
        g_boot_options_ui.chime_quiet_end_options,
        chime_quiet_end_event, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *wifi_page =
        g_boot_options_ui.pages[BOOT_OPTIONS_WIFI];
    g_boot_options_ui.wifi_enabled_options =
        lv_buttonmatrix_create(wifi_page);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.wifi_enabled_options, g_wifi_enabled_map);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.wifi_enabled_options,
        LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl_all(
        g_boot_options_ui.wifi_enabled_options,
        LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
    lv_buttonmatrix_set_one_checked(
        g_boot_options_ui.wifi_enabled_options, true);
    lv_obj_set_size(
        g_boot_options_ui.wifi_enabled_options, 260, 36);
    lv_obj_align(
        g_boot_options_ui.wifi_enabled_options,
        LV_ALIGN_TOP_MID, 0, 0);
    style_boot_options_matrix(
        g_boot_options_ui.wifi_enabled_options);
    lv_obj_add_event_cb(
        g_boot_options_ui.wifi_enabled_options,
        wifi_enabled_event, LV_EVENT_VALUE_CHANGED, nullptr);

    g_boot_options_ui.wifi_status = lv_label_create(wifi_page);
    lv_label_set_text(
        g_boot_options_ui.wifi_status,
        tr("Wi-Fi disabled\nClock remains fully offline"));
    lv_obj_set_width(g_boot_options_ui.wifi_status, 260);
    lv_obj_set_style_text_font(
        g_boot_options_ui.wifi_status, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_align(
        g_boot_options_ui.wifi_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(
        g_boot_options_ui.wifi_status,
        LV_ALIGN_TOP_MID, 0, 45);

    lv_obj_t *wifi_setup_button =
        create_action_button(
            wifi_page, tr("Setup Wi-Fi"), boot_wifi_setup_event);
    g_boot_options_ui.wifi_setup_label =
        lv_obj_get_child(wifi_setup_button, 0);
    lv_obj_set_size(wifi_setup_button, 260, 46);
    lv_obj_align(
        wifi_setup_button, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *start_page =
        g_boot_options_ui.pages[BOOT_OPTIONS_START];
    lv_obj_t *clock_button =
        create_action_button(start_page, tr("Clock"),
                             boot_start_clock_event);
    g_boot_options_ui.clock_button_label =
        lv_obj_get_child(clock_button, 0);
    lv_label_set_text(
        g_boot_options_ui.clock_button_label, tr("Clock"));
    lv_obj_set_size(clock_button, 122, 124);
    lv_obj_align(clock_button, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *emulator_button =
        create_action_button(start_page, tr("Emulator"),
                             boot_start_emulator_event);
    g_boot_options_ui.emulator_button_label =
        lv_obj_get_child(emulator_button, 0);
    lv_obj_set_size(emulator_button, 122, 124);
    lv_obj_align(emulator_button, LV_ALIGN_RIGHT_MID, -8, 0);

    lv_obj_t *tools_page =
        g_boot_options_ui.pages[BOOT_OPTIONS_TOOLS];
    lv_obj_t *diagnostics_button =
        create_action_button(tools_page, tr("Diagnostics"),
                             boot_diagnostics_event);
    g_boot_options_ui.diagnostics_button_label =
        lv_obj_get_child(diagnostics_button, 0);
    lv_obj_set_size(diagnostics_button, 260, 58);
    lv_obj_align(diagnostics_button, LV_ALIGN_TOP_MID, 0, 0);

    g_boot_options_ui.rtc_status = lv_label_create(tools_page);
    lv_label_set_text(g_boot_options_ui.rtc_status, tr("RTC: checking..."));
    lv_obj_set_width(g_boot_options_ui.rtc_status, 260);
    lv_obj_set_style_text_font(g_boot_options_ui.rtc_status,
                               &lv_font_chicago_8, 0);
    lv_obj_set_style_text_align(g_boot_options_ui.rtc_status,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(
        g_boot_options_ui.rtc_status, LV_ALIGN_TOP_MID, 0, 72);

    lv_obj_t *calibration_label = lv_label_create(tools_page);
    g_boot_options_ui.calibration_label = calibration_label;
    lv_label_set_text(
        calibration_label, tr("Press Clock for screen calibration"));
    lv_obj_set_style_text_font(calibration_label, &lv_font_chicago_8, 0);
    lv_obj_align(calibration_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    g_boot_options_ui.previous =
        create_action_button(
            g_boot_options_ui.panel, tr("Previous"),
            boot_options_previous_event);
    lv_obj_set_size(g_boot_options_ui.previous, 84, 40);
    lv_obj_align(
        g_boot_options_ui.previous,
        LV_ALIGN_BOTTOM_LEFT, 0, 0);
    g_boot_options_ui.previous_label =
        lv_obj_get_child(g_boot_options_ui.previous, 0);

    g_boot_options_ui.exit =
        create_action_button(
            g_boot_options_ui.panel, tr("Exit"),
            boot_exit_event);
    lv_obj_set_size(g_boot_options_ui.exit, 84, 40);
    lv_obj_align(
        g_boot_options_ui.exit,
        LV_ALIGN_BOTTOM_MID, 0, 0);
    g_boot_options_ui.exit_label =
        lv_obj_get_child(g_boot_options_ui.exit, 0);

    g_boot_options_ui.next =
        create_action_button(
            g_boot_options_ui.panel, tr("Next"),
            boot_options_next_event);
    lv_obj_set_size(g_boot_options_ui.next, 84, 40);
    lv_obj_align(
        g_boot_options_ui.next,
        LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    g_boot_options_ui.next_label =
        lv_obj_get_child(g_boot_options_ui.next, 0);

    lv_obj_add_flag(g_boot_options_ui.panel, LV_OBJ_FLAG_HIDDEN);
    set_boot_options_page(BOOT_OPTIONS_START);
}

static void show_boot_options_ui()
{
    lv_buttonmatrix_clear_button_ctrl_all(g_boot_options_ui.brightness_options,
                                          LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_button_ctrl(g_boot_options_ui.brightness_options,
                                    (uint32_t)g_boot_brightness,
                                    LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_selected_button(
        g_boot_options_ui.brightness_options,
        (uint32_t)g_boot_brightness);

    lv_buttonmatrix_clear_button_ctrl_all(
        g_boot_options_ui.remember_selection,
        LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_button_ctrl(
        g_boot_options_ui.remember_selection,
        1, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_buttonmatrix_set_selected_button(
        g_boot_options_ui.remember_selection, 1);
    set_checked_button(
        g_boot_options_ui.date_format_options,
        (uint32_t)g_date_format);
    set_checked_button(
        g_boot_options_ui.temperature_unit_options,
        (uint32_t)g_temperature_unit);
    set_checked_button(
        g_boot_options_ui.clock_face_options,
        (uint32_t)g_clock_face);
    set_checked_button(
        g_boot_options_ui.clock_theme_options,
        (uint32_t)g_clock_theme);
    set_checked_button(
        g_boot_options_ui.screensaver_options,
        (uint32_t)g_screensaver_mode);
    set_checked_button(
        g_boot_options_ui.screensaver_delay_options,
        g_screensaver_delay_index);
    update_language_selection(true);
    update_night_options_ui();
    update_chime_options_ui();
    update_wifi_options_ui();
    set_boot_options_page(BOOT_OPTIONS_START);

    char rtc_status[64];
    if (format_rtc_health(rtc_status, sizeof(rtc_status)))
    {
        lv_obj_add_flag(g_boot_options_ui.rtc_status, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_label_set_text(g_boot_options_ui.rtc_status, rtc_status);
        lv_obj_clear_flag(g_boot_options_ui.rtc_status, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(g_boot_options_ui.panel, LV_OBJ_FLAG_HIDDEN);
}

static void init_diagnostics_ui(lv_obj_t *screen)
{
    g_diagnostics_ui.panel = lv_obj_create(screen);
    lv_obj_set_size(g_diagnostics_ui.panel, 286, 230);
    lv_obj_center(g_diagnostics_ui.panel);
    lv_obj_set_style_bg_color(g_diagnostics_ui.panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_diagnostics_ui.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_diagnostics_ui.panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_diagnostics_ui.panel, 2, 0);
    lv_obj_set_style_radius(g_diagnostics_ui.panel, 0, 0);
    lv_obj_set_style_pad_all(g_diagnostics_ui.panel, 6, 0);

    g_diagnostics_ui.title = lv_label_create(g_diagnostics_ui.panel);
    lv_label_set_text(g_diagnostics_ui.title, tr("Hardware Diagnostics"));
    lv_obj_set_style_text_font(g_diagnostics_ui.title, &lv_font_chicago_8, 0);
    lv_obj_align(g_diagnostics_ui.title, LV_ALIGN_TOP_MID, 0, 0);

    g_diagnostics_ui.status = lv_label_create(g_diagnostics_ui.panel);
    lv_label_set_text(g_diagnostics_ui.status, tr("Checking hardware..."));
    lv_obj_set_width(g_diagnostics_ui.status, lv_pct(100));
    lv_obj_set_style_text_font(g_diagnostics_ui.status,
                               &lv_font_chicago_8, 0);
    lv_obj_set_style_text_line_space(g_diagnostics_ui.status, 0, 0);
    lv_obj_align(g_diagnostics_ui.status, LV_ALIGN_TOP_LEFT, 0, 18);

    lv_obj_t *back_button =
        create_action_button(g_diagnostics_ui.panel, tr("Back"),
                             diagnostics_back_event);
    g_diagnostics_ui.back_label = lv_obj_get_child(back_button, 0);
    lv_obj_set_size(back_button, 80, 24);
    lv_obj_align(back_button, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_add_flag(g_diagnostics_ui.panel, LV_OBJ_FLAG_HIDDEN);
}

static void init_wifi_setup_ui(lv_obj_t *screen)
{
    g_wifi_setup_ui.panel = lv_obj_create(screen);
    lv_obj_set_size(g_wifi_setup_ui.panel, 286, 208);
    lv_obj_center(g_wifi_setup_ui.panel);
    lv_obj_set_style_bg_color(
        g_wifi_setup_ui.panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        g_wifi_setup_ui.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        g_wifi_setup_ui.panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_wifi_setup_ui.panel, 2, 0);
    lv_obj_set_style_radius(g_wifi_setup_ui.panel, 0, 0);
    lv_obj_set_style_pad_all(g_wifi_setup_ui.panel, 8, 0);

    g_wifi_setup_ui.title = lv_label_create(g_wifi_setup_ui.panel);
    lv_label_set_text(g_wifi_setup_ui.title, tr("Wi-Fi Setup"));
    lv_obj_set_style_text_font(g_wifi_setup_ui.title, &lv_font_chicago_8, 0);
    lv_obj_align(g_wifi_setup_ui.title, LV_ALIGN_TOP_MID, 0, 0);

    g_wifi_setup_ui.status =
        lv_label_create(g_wifi_setup_ui.panel);
    lv_label_set_text(
        g_wifi_setup_ui.status,
        tr("Connect to: Maclock Setup\nThen open: 192.168.4.1"));
    lv_obj_set_width(g_wifi_setup_ui.status, 250);
    lv_obj_set_style_text_font(
        g_wifi_setup_ui.status, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_align(
        g_wifi_setup_ui.status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(
        g_wifi_setup_ui.status, 4, 0);
    lv_obj_align(
        g_wifi_setup_ui.status, LV_ALIGN_TOP_MID, 0, 28);

    lv_obj_t *back_button =
        create_action_button(
            g_wifi_setup_ui.panel, tr("Back"),
            wifi_setup_back_event);
    g_wifi_setup_ui.back_label = lv_obj_get_child(back_button, 0);
    lv_obj_set_size(back_button, 100, 38);
    lv_obj_align(back_button, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_add_flag(
        g_wifi_setup_ui.panel, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_language_ui()
{
    update_boot_translation_maps();
    if (!g_boot_options_ui.panel)
        return;

    const uint32_t remember_selected =
        lv_buttonmatrix_get_selected_button(
            g_boot_options_ui.remember_selection);

    lv_buttonmatrix_set_map(
        g_boot_options_ui.brightness_options, g_brightness_map);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.remember_selection, g_remember_map);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.clock_face_options, g_clock_face_map);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.clock_theme_options, g_clock_theme_map);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.screensaver_options, g_screensaver_map);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.screensaver_delay_options,
        g_screensaver_delay_map);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.night_enabled_options, g_night_enabled_map);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.night_screen_options, g_night_screen_map);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.chime_mode_options, g_chime_mode_map);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.chime_quiet_options, g_chime_quiet_map);
    lv_buttonmatrix_set_map(
        g_boot_options_ui.wifi_enabled_options, g_wifi_enabled_map);

    lv_label_set_text(g_boot_options_ui.brightness_label, tr("Brightness"));
    lv_label_set_text(g_boot_options_ui.remember_label, tr("Default boot mode"));
    lv_label_set_text(
        g_boot_options_ui.date_format_label, tr("Date format"));
    lv_label_set_text(
        g_boot_options_ui.temperature_unit_label,
        tr("Temperature unit"));
    lv_label_set_text(
        g_boot_options_ui.screensaver_delay_label,
        tr("Start after"));
    lv_label_set_text(g_boot_options_ui.dim_from_label, tr("Dim from"));
    lv_label_set_text(g_boot_options_ui.normal_at_label, tr("Normal at"));
    lv_label_set_text(g_boot_options_ui.screen_off_label, tr("Screen off at"));
    lv_label_set_text(g_boot_options_ui.quiet_from_label, tr("Quiet from"));
    lv_label_set_text(g_boot_options_ui.quiet_end_label, tr("Quiet ends"));
    lv_label_set_text(g_boot_options_ui.wifi_setup_label, tr("Setup Wi-Fi"));
    lv_label_set_text(g_boot_options_ui.clock_button_label, tr("Clock"));
    lv_label_set_text(g_boot_options_ui.emulator_button_label, tr("Emulator"));
    lv_label_set_text(g_boot_options_ui.diagnostics_button_label, tr("Diagnostics"));
    lv_label_set_text(
        g_boot_options_ui.calibration_label,
        tr("Press Clock for screen calibration"));
    lv_label_set_text(g_boot_options_ui.previous_label, tr("Previous"));
    lv_label_set_text(g_boot_options_ui.exit_label, tr("Exit"));
    lv_label_set_text(g_boot_options_ui.next_label, tr("Next"));
    sound_selector_refresh_language(
        &g_boot_options_ui.chime_sound_selector);

    lv_label_set_text(g_diagnostics_ui.title, tr("Hardware Diagnostics"));
    lv_label_set_text(g_diagnostics_ui.back_label, tr("Back"));
    lv_label_set_text(g_wifi_setup_ui.title, tr("Wi-Fi Setup"));
    lv_label_set_text(g_wifi_setup_ui.back_label, tr("Back"));
    lv_label_set_text(
        g_wifi_setup_ui.status,
        tr("Connect to: Maclock Setup\nThen open: 192.168.4.1"));
    update_boot_message();
    update_menu_titles();
    lv_label_set_text(g_ui.clock_label, tr("Clock"));
    lv_label_set_text(
        g_clock_faces_ui.compact_title, tr("Clock"));
    lv_label_set_text(
        g_clock_faces_ui.flip_title, tr("Clock"));
    lv_label_set_text(g_calib_ui.label, tr("Touch the crosshair"));

    alarm_ui_refresh_language();
    timer_ui_refresh_language();
    datetime_ui_refresh_language();
    update_language_selection(true);
    set_checked_button(
        g_boot_options_ui.brightness_options,
        (uint32_t)g_boot_brightness);
    set_checked_button(
        g_boot_options_ui.remember_selection,
        remember_selected < 2 ? remember_selected : 1);
    set_checked_button(
        g_boot_options_ui.date_format_options,
        (uint32_t)g_date_format);
    set_checked_button(
        g_boot_options_ui.temperature_unit_options,
        (uint32_t)g_temperature_unit);
    set_checked_button(
        g_boot_options_ui.clock_face_options,
        (uint32_t)g_clock_face);
    set_checked_button(
        g_boot_options_ui.clock_theme_options,
        (uint32_t)g_clock_theme);
    set_checked_button(
        g_boot_options_ui.screensaver_options,
        (uint32_t)g_screensaver_mode);
    set_checked_button(
        g_boot_options_ui.screensaver_delay_options,
        g_screensaver_delay_index);
    update_night_options_ui();
    update_chime_options_ui();
    update_wifi_options_ui();
    set_boot_options_page(g_boot_options_page);
    update_diagnostics_ui();
}

static void hide_all_ui()
{
    lv_obj_add_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.disk_missing_1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.disk_missing_2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.boot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.menu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.menu_titles, LV_OBJ_FLAG_HIDDEN);
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
    lv_obj_add_flag(g_ui.alarm_indicator, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.white_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.black_line, LV_OBJ_FLAG_HIDDEN);
    if (g_clock_faces_ui.compact)
        lv_obj_add_flag(
            g_clock_faces_ui.compact, LV_OBJ_FLAG_HIDDEN);
    if (g_clock_faces_ui.analog)
        lv_obj_add_flag(
            g_clock_faces_ui.analog, LV_OBJ_FLAG_HIDDEN);
    if (g_clock_faces_ui.flip)
        lv_obj_add_flag(
            g_clock_faces_ui.flip, LV_OBJ_FLAG_HIDDEN);
    if (g_clock_faces_ui.screensaver)
        lv_obj_add_flag(
            g_clock_faces_ui.screensaver, LV_OBJ_FLAG_HIDDEN);
    if (g_boot_options_ui.panel)
        lv_obj_add_flag(g_boot_options_ui.panel, LV_OBJ_FLAG_HIDDEN);
    if (g_diagnostics_ui.panel)
        lv_obj_add_flag(g_diagnostics_ui.panel, LV_OBJ_FLAG_HIDDEN);
    if (g_wifi_setup_ui.panel)
        lv_obj_add_flag(g_wifi_setup_ui.panel, LV_OBJ_FLAG_HIDDEN);
    for (size_t i = 0; i < k_plugin_max; ++i)
    {
        if (g_ui.plugin_icons[i])
            lv_obj_add_flag(g_ui.plugin_icons[i], LV_OBJ_FLAG_HIDDEN);
    }
    datetime_ui_hide();
    alarm_ui_hide();
    timer_ui_hide();
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

static float display_temperature(float celsius)
{
    return g_temperature_unit == UI_TEMPERATURE_FAHRENHEIT
               ? celsius * 9.0f / 5.0f + 32.0f
               : celsius;
}

static char display_temperature_unit()
{
    return g_temperature_unit == UI_TEMPERATURE_FAHRENHEIT
               ? 'F'
               : 'C';
}

static void format_display_date(
    const DateTime &date, char *text, size_t text_size)
{
    switch (g_date_format)
    {
    case UI_DATE_FORMAT_MDY:
        snprintf(
            text, text_size, "%02d/%02d/%04d",
            date.month(), date.day(), date.year());
        break;
    case UI_DATE_FORMAT_YMD:
        snprintf(
            text, text_size, "%04d-%02d-%02d",
            date.year(), date.month(), date.day());
        break;
    default:
        snprintf(
            text, text_size, "%02d/%02d/%04d",
            date.day(), date.month(), date.year());
        break;
    }
}

static void update_clock_labels()
{
    static int last_sec = -1;
    static int16_t gauge_width = 0;
    static int16_t gauge_box_w = 0;
    DateTime now = rtc_now();
    int sec = now.second();
    if (sec == last_sec)
        return;
    last_sec = sec;

    char buf[24];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             now.hour(), now.minute(), sec);
    lv_label_set_text(g_ui.time, buf);
    lv_obj_align(g_ui.time, LV_ALIGN_TOP_MID, 0, 14 + 4);

    format_display_date(now, buf, sizeof(buf));
    lv_label_set_text(g_ui.date, buf);

    const WifiModeSnapshot online = wifi_mode_snapshot();

    float temperature = NAN;
    float gauge_value = 0.0f;
    float gauge_min = 0.0f;
    float gauge_max = 1.0f;
    bool sensor_valid = false;

    switch (g_weather_sensor)
    {
    case WEATHER_SENSOR_BMP5XX:
    {
        if (!bmp.performReading())
            break;
        temperature = bmp.temperature;
        const float p = bmp.pressure;
        gauge_value = p;
        gauge_min = 980.0f;
        gauge_max = 1040.0f;
        sensor_valid = true;
        if (p < 1000.0f)
            lv_image_set_src(g_ui.gauge_icon, "S:/rainy.png");
        else if (p < 1020.0f)
            lv_image_set_src(g_ui.gauge_icon, "S:/cloudy.png");
        else
            lv_image_set_src(g_ui.gauge_icon, "S:/sunny.png");
        break;
    }

    case WEATHER_SENSOR_HTU2X:
    {
        temperature = htu2x.readTemperature();
        const float humidity = htu2x.readHumidity();
        if (isnan(temperature) || isnan(humidity))
            break;
        gauge_value = humidity;
        gauge_min = 0.0f;
        gauge_max = 100.0f;
        sensor_valid = true;
        if (humidity >= 70.0f)
            lv_image_set_src(g_ui.gauge_icon, "S:/rainy.png");
        else if (humidity >= 40.0f)
            lv_image_set_src(g_ui.gauge_icon, "S:/cloudy.png");
        else
            lv_image_set_src(g_ui.gauge_icon, "S:/sunny.png");
        break;
    }

    default:
        break;
    }

    if (online.forecast_valid)
    {
        char internal[16];
        if (sensor_valid)
            snprintf(
                internal, sizeof(internal), "%.1f°",
                display_temperature(temperature));
        else
            snprintf(internal, sizeof(internal), "--");

        char weather[112];
        snprintf(
            weather, sizeof(weather),
            "%s: %s   %s: %.1f°   %s: %.0f-%.0f°%c",
            tr("In"), internal, tr("Out"),
            display_temperature(online.current_temperature),
            tr("Today"),
            display_temperature(online.minimum_temperature),
            display_temperature(online.maximum_temperature),
            display_temperature_unit());

        lv_label_set_text(g_ui.temp, weather);
        lv_obj_clear_flag(g_ui.temp, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(
            g_ui.date, &lv_font_chicago_32, 0);
        lv_obj_align(g_ui.date, LV_ALIGN_TOP_MID,
                     0, 14 + 4 + 32 + 16);
        lv_obj_set_style_text_font(
            g_ui.temp, &lv_font_chicago_8, 0);
        lv_obj_set_style_text_letter_space(g_ui.temp, 0, 0);
        lv_label_set_long_mode(g_ui.temp, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(g_ui.temp, 236);
        lv_obj_set_style_text_align(
            g_ui.temp, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(
            g_ui.temp, LV_ALIGN_BOTTOM_LEFT, 0, -3);
        lv_obj_align(
            g_ui.gauge_icon, LV_ALIGN_BOTTOM_RIGHT, -12, -3);

        if (online.weather_code <= 1)
            lv_image_set_src(g_ui.gauge_icon, "S:/sunny.png");
        else if (online.weather_code <= 3 ||
                 online.weather_code == 45 ||
                 online.weather_code == 48)
            lv_image_set_src(g_ui.gauge_icon, "S:/cloudy.png");
        else
            lv_image_set_src(g_ui.gauge_icon, "S:/rainy.png");
        lv_obj_add_flag(g_ui.gauge_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_ui.gauge_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(g_ui.temp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(
        g_ui.date, &lv_font_chicago_32, 0);
    lv_obj_set_style_text_font(
        g_ui.temp, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_letter_space(g_ui.temp, 1, 0);
    lv_obj_align(g_ui.date, LV_ALIGN_TOP_MID,
                 0, 14 + 4 + 32 + 16);
    lv_obj_set_width(g_ui.temp, 220);
    lv_obj_set_style_text_align(
        g_ui.temp, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(g_ui.temp, LV_ALIGN_TOP_LEFT, 12, 118);
    lv_obj_align(
        g_ui.gauge_icon, LV_ALIGN_TOP_RIGHT, -12, 111);

    if (!sensor_valid)
    {
        lv_label_set_text(g_ui.temp, "--");
        lv_obj_add_flag(g_ui.gauge_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_ui.gauge_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(g_ui.gauge_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_ui.gauge_box, LV_OBJ_FLAG_HIDDEN);

    char tbuf[12];
    snprintf(
        tbuf, sizeof(tbuf), "%02.1f°%c",
        display_temperature(temperature),
        display_temperature_unit());
    lv_label_set_text(g_ui.temp, tbuf);

    if (gauge_width == 0)
    {
        gauge_width = lv_obj_get_width(g_ui.gauge_line);
        gauge_box_w = lv_obj_get_width(g_ui.gauge_box);
    }

    float clamped = gauge_value;
    if (clamped < gauge_min)
        clamped = gauge_min;
    if (clamped > gauge_max)
        clamped = gauge_max;
    const float t = (clamped - gauge_min) / (gauge_max - gauge_min);
    const int16_t max_offset = gauge_width - gauge_box_w;
    const int16_t x_offset = (int16_t)(max_offset * t);
    lv_obj_align_to(g_ui.gauge_box, g_ui.gauge_line, LV_ALIGN_LEFT_MID, x_offset, 0);
    lv_obj_move_foreground(g_ui.gauge_box);
}

static void update_alarm_indicator_layout(bool active)
{
    static constexpr int kIndicatorGap = 4;
    static constexpr int kDateShift = (18 + kIndicatorGap) / 2;
    const int date_top = 14 + 4 + 32 + 16;

    lv_obj_align(g_ui.date, LV_ALIGN_TOP_MID,
                 active ? kDateShift : 0, date_top);
    if (!active)
    {
        lv_obj_add_flag(
            g_ui.alarm_indicator, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_update_layout(g_ui.date);
    lv_obj_align_to(g_ui.alarm_indicator, g_ui.date,
                    LV_ALIGN_OUT_LEFT_MID, -kIndicatorGap, 0);
    lv_obj_clear_flag(
        g_ui.alarm_indicator, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *create_clock_face_root(
    lv_obj_t *screen, lv_color_t color)
{
    lv_obj_t *root = lv_obj_create(screen);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, color, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
    return root;
}

static lv_obj_t *create_clock_face_label(
    lv_obj_t *parent, const lv_font_t *font,
    lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static void set_analog_hand(
    lv_obj_t *hand, lv_point_precise_t points[2],
    float angle_degrees, float length, float tail)
{
    static constexpr float kDegreesToRadians =
        3.14159265358979323846f / 180.0f;
    static constexpr float kCenter = 84.0f;
    const float angle = (angle_degrees - 90.0f) *
                        kDegreesToRadians;
    const float x = cosf(angle);
    const float y = sinf(angle);
    points[0].x = (lv_value_precise_t)lroundf(
        kCenter - x * tail);
    points[0].y = (lv_value_precise_t)lroundf(
        kCenter - y * tail);
    points[1].x = (lv_value_precise_t)lroundf(
        kCenter + x * length);
    points[1].y = (lv_value_precise_t)lroundf(
        kCenter + y * length);
    lv_line_set_points_mutable(hand, points, 2);
}

static lv_obj_t *create_flip_card(
    lv_obj_t *parent, int16_t x, int16_t width)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, width, 96);
    lv_obj_set_pos(card, x, 42);
    lv_obj_set_style_bg_color(card, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_black(), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hinge = lv_obj_create(card);
    lv_obj_remove_style_all(hinge);
    lv_obj_set_size(hinge, lv_pct(100), 1);
    lv_obj_align(hinge, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(hinge, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(hinge, LV_OPA_50, 0);
    return card;
}

static lv_obj_t *create_flip_flap(
    lv_obj_t *card, lv_obj_t **label, int16_t width)
{
    lv_obj_t *flap = lv_obj_create(card);
    lv_obj_remove_style_all(flap);
    lv_obj_set_size(flap, lv_pct(100), lv_pct(100));
    lv_obj_center(flap);
    lv_obj_set_style_bg_color(flap, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(flap, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(flap, lv_color_black(), 0);
    lv_obj_set_style_border_width(flap, 2, 0);
    lv_obj_set_style_radius(flap, 8, 0);
    lv_obj_set_style_transform_pivot_x(
        flap, width / 2, 0);
    lv_obj_set_style_transform_pivot_y(flap, 48, 0);
    lv_obj_set_style_transform_scale_y(
        flap, LV_SCALE_NONE, 0);
    lv_obj_remove_flag(flap, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hinge = lv_obj_create(flap);
    lv_obj_remove_style_all(hinge);
    lv_obj_set_size(hinge, lv_pct(100), 1);
    lv_obj_align(hinge, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(hinge, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(hinge, LV_OPA_50, 0);

    *label = create_clock_face_label(
        flap, &lv_font_chicago_48, lv_color_white());
    lv_label_set_text(*label, "00");
    lv_obj_center(*label);
    lv_obj_add_flag(flap, LV_OBJ_FLAG_HIDDEN);
    return flap;
}

static void flip_scale_y_animation(
    void *object, int32_t scale)
{
    lv_obj_set_style_transform_scale_y(
        (lv_obj_t *)object, scale, 0);
}

static void flip_expand_completed(lv_anim_t *animation)
{
    FlipCardAnimation *state =
        (FlipCardAnimation *)lv_anim_get_user_data(animation);
    if (!state)
        return;
    strlcpy(
        state->displayed, state->pending,
        sizeof(state->displayed));
    lv_label_set_text(state->label, state->displayed);
    lv_obj_set_style_transform_scale_y(
        state->flap, LV_SCALE_NONE, 0);
    lv_obj_add_flag(state->flap, LV_OBJ_FLAG_HIDDEN);
    state->animating = false;
}

static void flip_collapse_completed(lv_anim_t *animation)
{
    FlipCardAnimation *state =
        (FlipCardAnimation *)lv_anim_get_user_data(animation);
    if (!state)
        return;
    lv_label_set_text(state->flap_label, state->pending);

    lv_anim_t expand;
    lv_anim_init(&expand);
    lv_anim_set_var(&expand, state->flap);
    lv_anim_set_exec_cb(&expand, flip_scale_y_animation);
    lv_anim_set_values(&expand, 12, LV_SCALE_NONE);
    lv_anim_set_duration(&expand, 210);
    lv_anim_set_path_cb(&expand, lv_anim_path_ease_out);
    lv_anim_set_user_data(&expand, state);
    lv_anim_set_completed_cb(
        &expand, flip_expand_completed);
    lv_anim_start(&expand);
}

static void reset_flip_card_animation(
    FlipCardAnimation &state)
{
    if (!state.flap)
        return;
    lv_anim_delete(state.flap, flip_scale_y_animation);
    lv_obj_set_style_transform_scale_y(
        state.flap, LV_SCALE_NONE, 0);
    lv_obj_add_flag(state.flap, LV_OBJ_FLAG_HIDDEN);
    state.initialized = false;
    state.animating = false;
}

static void update_flip_card(
    FlipCardAnimation &state, const char *value)
{
    if (!state.initialized)
    {
        strlcpy(
            state.displayed, value,
            sizeof(state.displayed));
        strlcpy(
            state.pending, value,
            sizeof(state.pending));
        lv_label_set_text(state.label, value);
        lv_label_set_text(state.flap_label, value);
        state.initialized = true;
        return;
    }

    if (state.animating)
    {
        if (strcmp(value, state.pending) == 0)
            return;
        lv_anim_delete(state.flap, flip_scale_y_animation);
        lv_obj_set_style_transform_scale_y(
            state.flap, LV_SCALE_NONE, 0);
        lv_obj_add_flag(state.flap, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(state.label, state.displayed);
        state.animating = false;
    }
    if (strcmp(value, state.displayed) == 0)
        return;

    strlcpy(
        state.pending, value, sizeof(state.pending));
    lv_label_set_text(state.label, state.pending);
    lv_label_set_text(state.flap_label, state.displayed);
    lv_obj_set_style_transform_scale_y(
        state.flap, LV_SCALE_NONE, 0);
    lv_obj_clear_flag(state.flap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(state.flap);
    state.animating = true;

    lv_anim_t collapse;
    lv_anim_init(&collapse);
    lv_anim_set_var(&collapse, state.flap);
    lv_anim_set_exec_cb(&collapse, flip_scale_y_animation);
    lv_anim_set_values(&collapse, LV_SCALE_NONE, 12);
    lv_anim_set_duration(&collapse, 170);
    lv_anim_set_path_cb(&collapse, lv_anim_path_ease_in);
    lv_anim_set_user_data(&collapse, &state);
    lv_anim_set_completed_cb(
        &collapse, flip_collapse_completed);
    lv_anim_start(&collapse);
}

static void apply_clock_face_theme()
{
    const bool dark = g_clock_theme == CLOCK_THEME_DARK;
    const lv_color_t background =
        dark ? lv_color_black() : lv_color_white();
    const lv_color_t foreground =
        dark ? lv_color_white() : lv_color_black();
    const lv_color_t card_background =
        dark ? lv_color_hex(0x181818) : lv_color_black();
    const lv_color_t card_border =
        dark ? lv_color_hex(0x707070) : lv_color_black();

    lv_obj_set_style_bg_color(
        g_clock_faces_ui.compact, background, 0);
    lv_obj_set_style_text_color(
        g_clock_faces_ui.compact_title, foreground, 0);
    lv_obj_set_style_text_color(
        g_clock_faces_ui.compact_time, foreground, 0);
    lv_obj_set_style_text_color(
        g_clock_faces_ui.compact_date, foreground, 0);
    lv_obj_set_style_text_color(
        g_clock_faces_ui.compact_weather, foreground, 0);

    lv_obj_set_style_bg_color(
        g_clock_faces_ui.analog, background, 0);
    lv_obj_set_style_bg_color(
        g_clock_faces_ui.analog_dial, background, 0);
    lv_obj_set_style_border_color(
        g_clock_faces_ui.analog_dial, foreground, 0);
    for (lv_obj_t *number : g_clock_faces_ui.analog_numbers)
        lv_obj_set_style_text_color(number, foreground, 0);
    lv_obj_set_style_line_color(
        g_clock_faces_ui.analog_hour_hand, foreground, 0);
    lv_obj_set_style_line_color(
        g_clock_faces_ui.analog_minute_hand, foreground, 0);
    lv_obj_set_style_line_color(
        g_clock_faces_ui.analog_second_hand, foreground, 0);
    lv_obj_set_style_bg_color(
        g_clock_faces_ui.analog_center, foreground, 0);
    lv_obj_set_style_text_color(
        g_clock_faces_ui.analog_date, foreground, 0);

    lv_obj_set_style_bg_color(
        g_clock_faces_ui.flip, background, 0);
    lv_obj_set_style_text_color(
        g_clock_faces_ui.flip_title, foreground, 0);
    lv_obj_set_style_bg_color(
        g_clock_faces_ui.flip_colon_top, foreground, 0);
    lv_obj_set_style_bg_color(
        g_clock_faces_ui.flip_colon_bottom, foreground, 0);
    lv_obj_set_style_text_color(
        g_clock_faces_ui.flip_date, foreground, 0);

    for (FlipCardAnimation &card :
         g_clock_faces_ui.flip_animations)
    {
        lv_obj_set_style_bg_color(
            card.card, card_background, 0);
        lv_obj_set_style_border_color(
            card.card, card_border, 0);
        lv_obj_set_style_bg_color(
            card.flap, card_background, 0);
        lv_obj_set_style_border_color(
            card.flap, card_border, 0);
        lv_obj_set_style_text_color(
            card.label, lv_color_white(), 0);
        lv_obj_set_style_text_color(
            card.flap_label, lv_color_white(), 0);
    }
}

static void init_clock_faces_ui(lv_obj_t *screen)
{
    g_clock_faces_ui.compact =
        create_clock_face_root(screen, lv_color_white());
    g_clock_faces_ui.compact_title =
        create_clock_face_label(
            g_clock_faces_ui.compact,
            &lv_font_chicago_8, lv_color_black());
    lv_label_set_text(
        g_clock_faces_ui.compact_title, tr("Clock"));
    lv_obj_align(
        g_clock_faces_ui.compact_title,
        LV_ALIGN_TOP_MID, 0, 13);

    g_clock_faces_ui.compact_time =
        create_clock_face_label(
            g_clock_faces_ui.compact,
            &lv_font_chicago_48, lv_color_black());
    lv_label_set_text(
        g_clock_faces_ui.compact_time, "00:00:00");
    lv_obj_set_width(g_clock_faces_ui.compact_time, 300);
    lv_obj_align(
        g_clock_faces_ui.compact_time,
        LV_ALIGN_TOP_MID, 0, 38);

    g_clock_faces_ui.compact_date =
        create_clock_face_label(
            g_clock_faces_ui.compact,
            &lv_font_chicago_32, lv_color_black());
    lv_label_set_text(
        g_clock_faces_ui.compact_date, "00/00/0000");
    lv_obj_set_width(g_clock_faces_ui.compact_date, 300);
    lv_obj_align(
        g_clock_faces_ui.compact_date,
        LV_ALIGN_TOP_MID, 0, 112);

    g_clock_faces_ui.compact_weather =
        create_clock_face_label(
            g_clock_faces_ui.compact,
            &lv_font_chicago_8, lv_color_black());
    lv_label_set_text(
        g_clock_faces_ui.compact_weather, "--");
    lv_obj_set_width(g_clock_faces_ui.compact_weather, 292);
    lv_label_set_long_mode(
        g_clock_faces_ui.compact_weather,
        LV_LABEL_LONG_CLIP);
    lv_obj_align(
        g_clock_faces_ui.compact_weather,
        LV_ALIGN_BOTTOM_MID, 0, -14);

    g_clock_faces_ui.analog =
        create_clock_face_root(screen, lv_color_white());
    g_clock_faces_ui.analog_dial =
        lv_obj_create(g_clock_faces_ui.analog);
    lv_obj_remove_style_all(g_clock_faces_ui.analog_dial);
    lv_obj_set_size(g_clock_faces_ui.analog_dial, 170, 170);
    lv_obj_align(
        g_clock_faces_ui.analog_dial,
        LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_bg_color(
        g_clock_faces_ui.analog_dial, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        g_clock_faces_ui.analog_dial, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        g_clock_faces_ui.analog_dial, lv_color_black(), 0);
    lv_obj_set_style_border_width(
        g_clock_faces_ui.analog_dial, 2, 0);
    lv_obj_set_style_radius(
        g_clock_faces_ui.analog_dial, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(
        g_clock_faces_ui.analog_dial,
        LV_OBJ_FLAG_SCROLLABLE);

    for (size_t i = 0; i < 12; ++i)
    {
        char number[3];
        snprintf(number, sizeof(number), "%u",
                 (unsigned)i + 1);
        lv_obj_t *label = create_clock_face_label(
            g_clock_faces_ui.analog_dial,
            &lv_font_chicago_8, lv_color_black());
        g_clock_faces_ui.analog_numbers[i] = label;
        lv_label_set_text(label, number);
        const float angle =
            (float)(i + 1) * 30.0f *
            3.14159265358979323846f / 180.0f;
        lv_obj_align(
            label, LV_ALIGN_CENTER,
            (int16_t)lroundf(sinf(angle) * 68.0f),
            (int16_t)lroundf(-cosf(angle) * 68.0f));
    }

    g_clock_faces_ui.analog_hour_hand =
        lv_line_create(g_clock_faces_ui.analog_dial);
    lv_obj_set_style_line_color(
        g_clock_faces_ui.analog_hour_hand,
        lv_color_black(), 0);
    lv_obj_set_style_line_width(
        g_clock_faces_ui.analog_hour_hand, 5, 0);
    lv_obj_set_style_line_rounded(
        g_clock_faces_ui.analog_hour_hand, true, 0);

    g_clock_faces_ui.analog_minute_hand =
        lv_line_create(g_clock_faces_ui.analog_dial);
    lv_obj_set_style_line_color(
        g_clock_faces_ui.analog_minute_hand,
        lv_color_black(), 0);
    lv_obj_set_style_line_width(
        g_clock_faces_ui.analog_minute_hand, 3, 0);
    lv_obj_set_style_line_rounded(
        g_clock_faces_ui.analog_minute_hand, true, 0);

    g_clock_faces_ui.analog_second_hand =
        lv_line_create(g_clock_faces_ui.analog_dial);
    lv_obj_set_style_line_color(
        g_clock_faces_ui.analog_second_hand,
        lv_color_black(), 0);
    lv_obj_set_style_line_width(
        g_clock_faces_ui.analog_second_hand, 1, 0);

    set_analog_hand(
        g_clock_faces_ui.analog_hour_hand,
        g_clock_faces_ui.analog_hour_points, 0, 43, 5);
    set_analog_hand(
        g_clock_faces_ui.analog_minute_hand,
        g_clock_faces_ui.analog_minute_points, 0, 61, 6);
    set_analog_hand(
        g_clock_faces_ui.analog_second_hand,
        g_clock_faces_ui.analog_second_points, 0, 68, 8);

    g_clock_faces_ui.analog_center = lv_obj_create(
        g_clock_faces_ui.analog_dial);
    lv_obj_remove_style_all(g_clock_faces_ui.analog_center);
    lv_obj_set_size(g_clock_faces_ui.analog_center, 8, 8);
    lv_obj_set_style_bg_color(
        g_clock_faces_ui.analog_center, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(
        g_clock_faces_ui.analog_center, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(
        g_clock_faces_ui.analog_center,
        LV_RADIUS_CIRCLE, 0);
    lv_obj_center(g_clock_faces_ui.analog_center);

    g_clock_faces_ui.analog_date =
        create_clock_face_label(
            g_clock_faces_ui.analog,
            &lv_font_chicago_32, lv_color_black());
    lv_label_set_text(
        g_clock_faces_ui.analog_date, "00/00/0000");
    lv_obj_set_width(g_clock_faces_ui.analog_date, 300);
    lv_obj_align(
        g_clock_faces_ui.analog_date,
        LV_ALIGN_BOTTOM_MID, 0, -8);

    g_clock_faces_ui.flip =
        create_clock_face_root(screen, lv_color_white());
    g_clock_faces_ui.flip_title = create_clock_face_label(
        g_clock_faces_ui.flip,
        &lv_font_chicago_8, lv_color_black());
    lv_label_set_text(
        g_clock_faces_ui.flip_title, tr("Clock"));
    lv_obj_align(
        g_clock_faces_ui.flip_title,
        LV_ALIGN_TOP_MID, 0, 13);

    static constexpr int16_t kFlipCardWidth = 56;
    static constexpr int16_t kFlipCardX[kFlipDigitCount] = {
        22, 82, 166, 226};
    for (size_t i = 0; i < kFlipDigitCount; ++i)
    {
        g_clock_faces_ui.flip_cards[i] =
            create_flip_card(
                g_clock_faces_ui.flip,
                kFlipCardX[i], kFlipCardWidth);
        g_clock_faces_ui.flip_digits[i] =
            create_clock_face_label(
                g_clock_faces_ui.flip_cards[i],
                &lv_font_chicago_48, lv_color_white());
        lv_label_set_text(
            g_clock_faces_ui.flip_digits[i], "0");
        lv_obj_center(g_clock_faces_ui.flip_digits[i]);

        FlipCardAnimation &animation =
            g_clock_faces_ui.flip_animations[i];
        animation.card = g_clock_faces_ui.flip_cards[i];
        animation.label = g_clock_faces_ui.flip_digits[i];
        animation.flap = create_flip_flap(
            animation.card, &animation.flap_label,
            kFlipCardWidth);
    }

    g_clock_faces_ui.flip_colon =
        lv_obj_create(g_clock_faces_ui.flip);
    lv_obj_remove_style_all(g_clock_faces_ui.flip_colon);
    lv_obj_set_size(g_clock_faces_ui.flip_colon, 16, 96);
    lv_obj_set_pos(g_clock_faces_ui.flip_colon, 144, 42);
    lv_obj_remove_flag(
        g_clock_faces_ui.flip_colon,
        LV_OBJ_FLAG_SCROLLABLE);

    g_clock_faces_ui.flip_colon_top =
        lv_obj_create(g_clock_faces_ui.flip_colon);
    lv_obj_remove_style_all(
        g_clock_faces_ui.flip_colon_top);
    lv_obj_set_size(
        g_clock_faces_ui.flip_colon_top, 4, 4);
    lv_obj_set_pos(
        g_clock_faces_ui.flip_colon_top, 6, 35);
    lv_obj_set_style_bg_color(
        g_clock_faces_ui.flip_colon_top,
        lv_color_black(), 0);
    lv_obj_set_style_bg_opa(
        g_clock_faces_ui.flip_colon_top,
        LV_OPA_COVER, 0);
    lv_obj_set_style_radius(
        g_clock_faces_ui.flip_colon_top,
        LV_RADIUS_CIRCLE, 0);

    g_clock_faces_ui.flip_colon_bottom =
        lv_obj_create(g_clock_faces_ui.flip_colon);
    lv_obj_remove_style_all(
        g_clock_faces_ui.flip_colon_bottom);
    lv_obj_set_size(
        g_clock_faces_ui.flip_colon_bottom, 4, 4);
    lv_obj_set_pos(
        g_clock_faces_ui.flip_colon_bottom, 6, 57);
    lv_obj_set_style_bg_color(
        g_clock_faces_ui.flip_colon_bottom,
        lv_color_black(), 0);
    lv_obj_set_style_bg_opa(
        g_clock_faces_ui.flip_colon_bottom,
        LV_OPA_COVER, 0);
    lv_obj_set_style_radius(
        g_clock_faces_ui.flip_colon_bottom,
        LV_RADIUS_CIRCLE, 0);

    g_clock_faces_ui.flip_date =
        create_clock_face_label(
            g_clock_faces_ui.flip,
            &lv_font_chicago_32, lv_color_black());
    lv_label_set_text(
        g_clock_faces_ui.flip_date, "00/00/0000");
    lv_obj_set_width(g_clock_faces_ui.flip_date, 300);
    lv_obj_align(
        g_clock_faces_ui.flip_date,
        LV_ALIGN_BOTTOM_MID, 0, -16);

    g_clock_faces_ui.screensaver =
        create_clock_face_root(screen, lv_color_black());
    for (size_t i = 0; i < kScreensaverStarCount; ++i)
    {
        lv_obj_t *star =
            lv_obj_create(g_clock_faces_ui.screensaver);
        lv_obj_remove_style_all(star);
        const uint8_t size = (i % 5 == 0) ? 2 : 1;
        lv_obj_set_size(star, size, size);
        lv_obj_set_style_bg_color(star, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(star, LV_OPA_COVER, 0);
        g_clock_faces_ui.screensaver_stars[i] = star;
        g_clock_faces_ui.screensaver_star_x[i] =
            (int16_t)((i * 47 + 13) % 304);
        g_clock_faces_ui.screensaver_star_y[i] =
            (int16_t)((i * 83 + 7) % 224);
        g_clock_faces_ui.screensaver_star_speed[i] =
            (uint8_t)(1 + i % 3);
        lv_obj_set_pos(
            star,
            g_clock_faces_ui.screensaver_star_x[i],
            g_clock_faces_ui.screensaver_star_y[i]);
    }

    g_clock_faces_ui.screensaver_clock =
        lv_obj_create(g_clock_faces_ui.screensaver);
    lv_obj_remove_style_all(
        g_clock_faces_ui.screensaver_clock);
    lv_obj_set_size(
        g_clock_faces_ui.screensaver_clock, 144, 66);
    lv_obj_set_style_bg_color(
        g_clock_faces_ui.screensaver_clock,
        lv_color_black(), 0);
    lv_obj_set_style_bg_opa(
        g_clock_faces_ui.screensaver_clock,
        LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(
        g_clock_faces_ui.screensaver_clock,
        lv_color_white(), 0);
    lv_obj_set_style_border_width(
        g_clock_faces_ui.screensaver_clock, 1, 0);
    lv_obj_set_style_radius(
        g_clock_faces_ui.screensaver_clock, 4, 0);
    lv_obj_remove_flag(
        g_clock_faces_ui.screensaver_clock,
        LV_OBJ_FLAG_SCROLLABLE);

    g_clock_faces_ui.screensaver_time =
        create_clock_face_label(
            g_clock_faces_ui.screensaver_clock,
            &lv_font_chicago_48, lv_color_white());
    lv_label_set_text(
        g_clock_faces_ui.screensaver_time, "00:00");
    lv_obj_center(g_clock_faces_ui.screensaver_time);
    g_clock_faces_ui.screensaver_clock_x = 8;
    g_clock_faces_ui.screensaver_clock_y = 8;
    g_clock_faces_ui.screensaver_clock_dx = 1;
    g_clock_faces_ui.screensaver_clock_dy = 1;
    lv_obj_set_pos(
        g_clock_faces_ui.screensaver_clock,
        g_clock_faces_ui.screensaver_clock_x,
        g_clock_faces_ui.screensaver_clock_y);

    apply_clock_face_theme();
}

static void show_selected_clock_face()
{
    hide_all_ui();
    g_screensaver_active = false;

    if (g_clock_face == CLOCK_FACE_MACINTOSH)
    {
        lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.white_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.black_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.menu_titles, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.menu_right, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.clock, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.clock_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.time, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.date, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.temp, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.gauge_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.gauge_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.gauge_box, LV_OBJ_FLAG_HIDDEN);
        update_alarm_indicator_layout(
            alarms_have_active_indicator());
        lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_t *face = nullptr;
    if (g_clock_face == CLOCK_FACE_COMPACT)
        face = g_clock_faces_ui.compact;
    else if (g_clock_face == CLOCK_FACE_ANALOG)
        face = g_clock_faces_ui.analog;
    else
    {
        face = g_clock_faces_ui.flip;
        for (FlipCardAnimation &animation :
             g_clock_faces_ui.flip_animations)
        {
            reset_flip_card_animation(animation);
        }
    }
    lv_obj_clear_flag(face, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(face);
}

static void update_selected_clock_face(unsigned long now_ms)
{
    update_clock_labels();
    if (g_clock_face == CLOCK_FACE_MACINTOSH)
    {
        if (timer_is_active())
        {
            char remaining[12];
            timer_format_remaining(
                now_ms, remaining, sizeof(remaining));
            lv_label_set_text(g_ui.date, remaining);
        }
        update_alarm_indicator_layout(
            alarms_have_active_indicator());
        return;
    }

    const DateTime current = rtc_now();
    char time_text[16];
    char footer[24];
    if (timer_is_active())
        timer_format_remaining(
            now_ms, footer, sizeof(footer));
    else
        format_display_date(current, footer, sizeof(footer));

    if (g_clock_face == CLOCK_FACE_COMPACT)
    {
        snprintf(
            time_text, sizeof(time_text), "%02d:%02d:%02d",
            current.hour(), current.minute(), current.second());
        lv_label_set_text(
            g_clock_faces_ui.compact_time, time_text);
        lv_label_set_text(
            g_clock_faces_ui.compact_date, footer);
        lv_label_set_text(
            g_clock_faces_ui.compact_weather,
            lv_label_get_text(g_ui.temp));
    }
    else if (g_clock_face == CLOCK_FACE_ANALOG)
    {
        const float seconds = (float)current.second();
        const float minutes =
            (float)current.minute() + seconds / 60.0f;
        const float hours =
            (float)(current.hour() % 12) + minutes / 60.0f;
        set_analog_hand(
            g_clock_faces_ui.analog_hour_hand,
            g_clock_faces_ui.analog_hour_points,
            hours * 30.0f, 43, 5);
        set_analog_hand(
            g_clock_faces_ui.analog_minute_hand,
            g_clock_faces_ui.analog_minute_points,
            minutes * 6.0f, 61, 6);
        set_analog_hand(
            g_clock_faces_ui.analog_second_hand,
            g_clock_faces_ui.analog_second_points,
            seconds * 6.0f, 68, 8);
        lv_label_set_text(
            g_clock_faces_ui.analog_date, footer);
    }
    else
    {
        snprintf(
            time_text, sizeof(time_text), "%02d%02d",
            current.hour(), current.minute());
        for (size_t i = 0; i < kFlipDigitCount; ++i)
        {
            const char digit[2] = {time_text[i], '\0'};
            update_flip_card(
                g_clock_faces_ui.flip_animations[i],
                digit);
        }
        const lv_opa_t colon_opa =
            current.second() % 2 ? LV_OPA_40 : LV_OPA_COVER;
        lv_obj_set_style_bg_opa(
            g_clock_faces_ui.flip_colon_top,
            colon_opa, 0);
        lv_obj_set_style_bg_opa(
            g_clock_faces_ui.flip_colon_bottom,
            colon_opa, 0);
        lv_label_set_text(
            g_clock_faces_ui.flip_date, footer);
    }
}

static void show_after_dark_screensaver()
{
    hide_all_ui();
    g_screensaver_active = true;
    if (g_cursor)
        lv_obj_add_flag(g_cursor, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(
        g_clock_faces_ui.screensaver, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_clock_faces_ui.screensaver);
}

static void update_after_dark_screensaver()
{
    static unsigned long last_move_ms = 0;
    static int last_second = -1;
    const unsigned long now_ms = millis();
    const DateTime current = rtc_now();
    if (current.second() != last_second)
    {
        char time_text[8];
        snprintf(
            time_text, sizeof(time_text), "%02d:%02d",
            current.hour(), current.minute());
        lv_label_set_text(
            g_clock_faces_ui.screensaver_time, time_text);
        last_second = current.second();
    }
    if (last_move_ms && now_ms - last_move_ms < 60)
        return;
    last_move_ms = now_ms;

    for (size_t i = 0; i < kScreensaverStarCount; ++i)
    {
        int16_t y =
            g_clock_faces_ui.screensaver_star_y[i] +
            g_clock_faces_ui.screensaver_star_speed[i];
        if (y >= 224)
        {
            y = 0;
            g_clock_faces_ui.screensaver_star_x[i] =
                (int16_t)(
                    (g_clock_faces_ui.screensaver_star_x[i] +
                     73 + i * 11) %
                    304);
        }
        g_clock_faces_ui.screensaver_star_y[i] = y;
        lv_obj_set_pos(
            g_clock_faces_ui.screensaver_stars[i],
            g_clock_faces_ui.screensaver_star_x[i], y);
    }

    int16_t x = g_clock_faces_ui.screensaver_clock_x +
                g_clock_faces_ui.screensaver_clock_dx;
    int16_t y = g_clock_faces_ui.screensaver_clock_y +
                g_clock_faces_ui.screensaver_clock_dy;
    if (x <= 0 || x >= 160)
    {
        g_clock_faces_ui.screensaver_clock_dx =
            -g_clock_faces_ui.screensaver_clock_dx;
        if (x < 0)
            x = 0;
        else if (x > 160)
            x = 160;
    }
    if (y <= 0 || y >= 158)
    {
        g_clock_faces_ui.screensaver_clock_dy =
            -g_clock_faces_ui.screensaver_clock_dy;
        if (y < 0)
            y = 0;
        else if (y > 158)
            y = 158;
    }
    g_clock_faces_ui.screensaver_clock_x = x;
    g_clock_faces_ui.screensaver_clock_y = y;
    lv_obj_set_pos(
        g_clock_faces_ui.screensaver_clock, x, y);
}

static void setup_weather_sensor()
{
    if (bmp.begin(BMP5XX_ALTERNATIVE_ADDRESS, &Wire))
    {
        g_weather_sensor = WEATHER_SENSOR_BMP5XX;
        g_weather_sensor_address = BMP5XX_ALTERNATIVE_ADDRESS;
        bmp.setTemperatureOversampling(BMP5XX_OVERSAMPLING_16X);
        bmp.setPressureOversampling(BMP5XX_OVERSAMPLING_16X);
        bmp.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_127);
        bmp.setOutputDataRate(BMP5XX_ODR_120_HZ);
        bmp.setPowerMode(BMP5XX_POWERMODE_NORMAL);
        bmp.enablePressure(true);
        Serial.println("BMP5xx detected at 0x47");
        return;
    }

    if (htu2x.begin(&Wire))
    {
        g_weather_sensor = WEATHER_SENSOR_HTU2X;
        g_weather_sensor_address = HTU21DF_I2CADDR;
        Serial.println("HTU2x detected at 0x40");
        return;
    }

    Serial.println("No weather sensor detected");
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
    g_ui.alarm_indicator_buf =
        load_png_once("S:/alarm_indicator.png");
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

    g_ui.menu_titles = lv_label_create(scr);
    lv_obj_remove_style_all(g_ui.menu_titles);
    lv_obj_set_size(g_ui.menu_titles, 251, 19);
    lv_obj_set_pos(g_ui.menu_titles, 37, 0);
    lv_obj_set_style_bg_color(
        g_ui.menu_titles, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(
        g_ui.menu_titles, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(
        g_ui.menu_titles, lv_color_black(), 0);
    lv_obj_set_style_text_font(
        g_ui.menu_titles, &lv_font_chicago_8, 0);
    lv_obj_set_style_pad_left(g_ui.menu_titles, 4, 0);
    lv_obj_set_style_pad_top(g_ui.menu_titles, 2, 0);
    lv_label_set_long_mode(
        g_ui.menu_titles, LV_LABEL_LONG_CLIP);
    update_menu_titles();

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
    lv_label_set_text(g_ui.clock_label, tr("Clock"));
    lv_obj_set_style_text_font(g_ui.clock_label, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_letter_space(g_ui.clock_label, 1, 0);
    lv_obj_set_style_bg_color(g_ui.clock_label, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_ui.clock_label, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(g_ui.clock_label, 12, 0);
    lv_obj_set_style_pad_right(g_ui.clock_label, 12, 0);
    lv_obj_align(g_ui.clock_label, LV_ALIGN_TOP_MID, 0, 2);

    g_ui.time = lv_label_create(g_ui.clock);
    lv_label_set_text(g_ui.time, "00:00:00");
    lv_obj_set_style_text_font(g_ui.time, &lv_font_chicago_48, 0);
    lv_obj_set_style_text_letter_space(g_ui.time, 1, 0);
    lv_obj_align(g_ui.time, LV_ALIGN_TOP_MID, 0, 8);

    g_ui.date = lv_label_create(g_ui.clock);
    lv_label_set_text(
        g_ui.date,
        g_date_format == UI_DATE_FORMAT_YMD
            ? "0000-00-00"
            : "00/00/0000");
    lv_obj_set_style_text_font(g_ui.date, &lv_font_chicago_32, 0);
    lv_obj_set_style_text_letter_space(g_ui.date, 1, 0);
    lv_obj_align(g_ui.date, LV_ALIGN_TOP_MID, 0, 83);

    g_ui.temp = lv_label_create(g_ui.clock);
    char temperature_placeholder[12];
    snprintf(
        temperature_placeholder,
        sizeof(temperature_placeholder),
        "--.-°%c", display_temperature_unit());
    lv_label_set_text(g_ui.temp, temperature_placeholder);
    lv_obj_set_style_text_font(g_ui.temp, &lv_font_chicago_8, 0);
    lv_obj_set_style_text_letter_space(g_ui.temp, 1, 0);
    lv_obj_set_width(g_ui.temp, 220);
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

    g_ui.alarm_indicator = lv_image_create(g_ui.clock);
    set_image_src(g_ui.alarm_indicator,
                  g_ui.alarm_indicator_buf,
                  "S:/alarm_indicator.png");
    lv_obj_add_flag(
        g_ui.alarm_indicator, LV_OBJ_FLAG_HIDDEN);

    g_ui.disk_missing_1 = lv_image_create(scr);
    set_image_src(g_ui.disk_missing_1, g_ui.disk_missing_1_buf, "S:/disk_missing_1.png");
    lv_obj_center(g_ui.disk_missing_1);

    g_ui.disk_missing_2 = lv_image_create(scr);
    set_image_src(g_ui.disk_missing_2, g_ui.disk_missing_2_buf, "S:/disk_missing_2.png");
    lv_obj_center(g_ui.disk_missing_2);

    g_ui.boot = lv_image_create(scr);
    set_image_src(g_ui.boot, g_ui.boot_buf, "S:/boot.png");
    lv_obj_center(g_ui.boot);

    g_ui.boot_message = lv_label_create(g_ui.boot);
    lv_obj_remove_style_all(g_ui.boot_message);
    lv_obj_set_size(g_ui.boot_message, 188, 24);
    lv_obj_set_pos(g_ui.boot_message, 68, 25);
    lv_obj_set_style_bg_opa(
        g_ui.boot_message, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(
        g_ui.boot_message, lv_color_white(), 0);
    lv_obj_set_style_text_color(
        g_ui.boot_message, lv_color_black(), 0);
    lv_obj_set_style_text_font(
        g_ui.boot_message, &lv_font_chicago_8, 0);
    lv_obj_set_style_pad_left(g_ui.boot_message, 6, 0);
    lv_obj_set_style_pad_top(g_ui.boot_message, 2, 0);
    lv_label_set_long_mode(
        g_ui.boot_message, LV_LABEL_LONG_CLIP);
    update_boot_message();

    for (size_t i = 0; i < k_plugin_max; ++i)
    {
        g_ui.plugin_icons[i] = lv_image_create(scr);
        set_image_src(g_ui.plugin_icons[i], g_ui.plugin_buf, "S:/plugin.png");
    }

    g_ui.corners = lv_image_create(scr);
    set_image_src(g_ui.corners, g_ui.corners_buf, "S:/corners.png");
    lv_obj_center(g_ui.corners);

    init_clock_faces_ui(scr);
    datetime_ui_init(scr);
    alarm_ui_init(scr);
    timer_ui_init(scr);

    g_calib_ui.label = lv_label_create(scr);
    lv_label_set_text(g_calib_ui.label, tr("Touch the crosshair"));
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

    init_boot_options_ui(scr);
    init_diagnostics_ui(scr);
    init_wifi_setup_ui(scr);

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
        bool running = false;
        if (g_mp3_lock)
            xSemaphoreTake(g_mp3_lock, portMAX_DELAY);
        if (mp3 && mp3->isRunning())
        {
            running = true;
            if (!mp3->loop())
            {
                mp3->stop();
                running = false;
                portENTER_CRITICAL(&g_mp3_mux);
                g_mp3_finished = true;
                portEXIT_CRITICAL(&g_mp3_mux);
            }
        }
        if (g_mp3_lock)
            xSemaphoreGive(g_mp3_lock);
        vTaskDelay(pdMS_TO_TICKS(running ? 1 : 10));
    }
}

static void run_emulator()
{
    stop_mp3_playback();
    wifi_mode_pause();
    if (g_audio_task_handle)
        vTaskSuspend(g_audio_task_handle);
    if (g_input_task_handle)
        vTaskSuspend(g_input_task_handle);

    minivmac();

    if (g_input_task_handle)
        vTaskResume(g_input_task_handle);
    if (g_audio_task_handle)
        vTaskResume(g_audio_task_handle);
    wifi_mode_resume();
}

static bool i2c_device_present(uint8_t addr)
{
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

static bool i2c_read_register(uint8_t addr, uint8_t reg, uint8_t &value)
{
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0)
        return false;
    if (Wire.requestFrom(addr, (uint8_t)1) != 1)
        return false;
    value = Wire.read();
    return true;
}

static bool i2c_write_register(uint8_t addr, uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

static bool probe_ds1307()
{
    static constexpr uint8_t k_rtc_address = 0x68;
    static constexpr uint8_t k_ds3231_control_register = 0x0E;
    static constexpr uint8_t k_ds3231_convert_temperature = 0x20;

    uint8_t original = 0;
    if (!i2c_read_register(k_rtc_address, k_ds3231_control_register, original))
        return false;
    if (!i2c_write_register(k_rtc_address, k_ds3231_control_register,
                            original | k_ds3231_convert_temperature))
        return false;

    delay(250);

    uint8_t after = 0;
    if (!i2c_read_register(k_rtc_address, k_ds3231_control_register, after))
        return false;

    const bool is_ds1307 = (after & k_ds3231_convert_temperature) != 0;
    i2c_write_register(k_rtc_address, k_ds3231_control_register,
                       is_ds1307 ? original : (original & ~k_ds3231_convert_temperature));
    return is_ds1307;
}

static bool setup_rtc()
{
    static constexpr uint8_t k_rtc_address = 0x68;
    if (!i2c_device_present(k_rtc_address))
    {
        Serial.println("No RTC detected at 0x68");
        return false;
    }

    if (probe_ds1307() && rtc_ds1307.begin(&Wire))
    {
        g_rtc_type = RTC_TYPE_DS1307;
        Serial.println("DS1307 detected at 0x68");
        return true;
    }

    if (rtc_ds3231.begin(&Wire))
    {
        g_rtc_type = RTC_TYPE_DS3231;
        Serial.println("DS3231 detected at 0x68");
        return true;
    }

    Serial.println("RTC at 0x68 could not be initialized");
    return false;
}

static void update_diagnostics_ui()
{
    static constexpr uint8_t addresses[] = {0x18, 0x38, 0x40, 0x47, 0x68};
    char i2c_devices[48] = {};
    size_t i2c_length = 0;
    for (uint8_t address : addresses)
    {
        if (!i2c_device_present(address))
            continue;
        const int written = snprintf(i2c_devices + i2c_length,
                                     sizeof(i2c_devices) - i2c_length,
                                     "%s0x%02X",
                                     i2c_length ? " " : "",
                                     address);
        if (written <= 0)
            break;
        i2c_length += (size_t)written;
        if (i2c_length >= sizeof(i2c_devices))
        {
            i2c_length = sizeof(i2c_devices) - 1;
            break;
        }
    }
    if (i2c_length == 0)
        snprintf(i2c_devices, sizeof(i2c_devices), "%s", tr("None"));

    char rtc_status[64];
    format_rtc_health(rtc_status, sizeof(rtc_status));

    const WifiModeSnapshot wifi = wifi_mode_snapshot();
    const char *network_state =
        !wifi.enabled
            ? tr("Disabled")
            : (wifi.portal_active
                   ? tr("Setup portal")
                   : (!wifi.configured
                          ? tr("Not configured")
                          : (wifi.connected
                                 ? tr("Online")
                                 : tr("Offline"))));
    const char *network_ssid =
        wifi.ssid[0] ? wifi.ssid : "--";
    char network_address[40];
    if (wifi.connected && wifi.ip_address[0])
    {
        snprintf(network_address, sizeof(network_address),
                 "%s / %ld dBm",
                 wifi.ip_address, (long)wifi.rssi);
    }
    else
    {
        snprintf(network_address, sizeof(network_address), "--");
    }

    char status[480];
    snprintf(status, sizeof(status),
             "%s: %s\n"
             "%s: %s\n"
             "%s: %s\n"
             "%s: %lld/%u\n"
             "%s: %s\n"
             "%s: %s\n"
             "%-7s: %s\n"
             "%-7s: %s\n"
             "%-7s: %s\n"
             "%-7s: %s\n"
             "%s",
             tr("Clock"),
             digitalRead(GPIO_CLOCK) == LOW
                 ? tr("Pressed")
                 : tr("Released"),
             tr("Alarm"),
             digitalRead(GPIO_ALARM) == LOW
                 ? tr("Pressed")
                 : tr("Released"),
             tr("Floppy"),
             digitalRead(GPIO_FLOPPY) == LOW
                 ? tr("Inserted")
                 : tr("Empty"),
             tr("Encoder"),
             (long long)encoder.getCount(), (unsigned)kBrightnessMax,
             tr("Touch"),
             touch.touched() ? tr("Pressed") : tr("Released"),
             tr("Charging"),
             digitalRead(GPIO_CHARGING) == HIGH ? tr("Yes") : tr("No"),
             tr("I2C"),
             i2c_devices,
             tr("Wi-Fi"),
             network_state,
             tr("SSID"),
             network_ssid,
             tr("IP/RSSI"),
             network_address,
             rtc_status);
    lv_label_set_text(g_diagnostics_ui.status, status);
}

void setup()
{
    Serial.begin(115200);
    analogWrite(TFT_BL_VAR, 0);
    preferences.begin("maclock", false);
    const uint8_t saved_language = preferences.getUChar(
        "language", UI_LANGUAGE_ENGLISH);
    localization_set_language(
        saved_language < UI_LANGUAGE_COUNT
            ? (UiLanguage)saved_language
            : UI_LANGUAGE_ENGLISH);
    const uint8_t saved_date_format =
        preferences.getUChar(
            "date_format", UI_DATE_FORMAT_DMY);
    g_date_format =
        saved_date_format < UI_DATE_FORMAT_COUNT
            ? (UiDateFormat)saved_date_format
            : UI_DATE_FORMAT_DMY;
    const uint8_t saved_temperature_unit =
        preferences.getUChar(
            "temp_unit", UI_TEMPERATURE_CELSIUS);
    g_temperature_unit =
        saved_temperature_unit < UI_TEMPERATURE_UNIT_COUNT
            ? (UiTemperatureUnit)saved_temperature_unit
            : UI_TEMPERATURE_CELSIUS;
    const uint8_t saved_clock_face =
        preferences.getUChar("clock_face", CLOCK_FACE_MACINTOSH);
    g_clock_face =
        saved_clock_face < CLOCK_FACE_COUNT
            ? (ClockFace)saved_clock_face
            : CLOCK_FACE_MACINTOSH;
    const uint8_t saved_clock_theme =
        preferences.getUChar("clock_theme", CLOCK_THEME_LIGHT);
    g_clock_theme =
        saved_clock_theme < CLOCK_THEME_COUNT
            ? (ClockTheme)saved_clock_theme
            : CLOCK_THEME_LIGHT;
    const uint8_t saved_screensaver_mode =
        preferences.getUChar("screen_mode", SCREENSAVER_OFF);
    g_screensaver_mode =
        saved_screensaver_mode < SCREENSAVER_MODE_COUNT
            ? (ScreensaverMode)saved_screensaver_mode
            : SCREENSAVER_OFF;
    g_screensaver_delay_index =
        preferences.getUChar("screen_delay", 1);
    if (g_screensaver_delay_index >=
        sizeof(g_screensaver_delays_minutes) /
            sizeof(g_screensaver_delays_minutes[0]))
    {
        g_screensaver_delay_index = 1;
    }
    datetime_ui_set_date_format(g_date_format);
    g_mp3_lock = xSemaphoreCreateMutex();
    alarms_init(preferences);
    wifi_mode_begin(preferences);

    uint8_t saved_boot_brightness =
        preferences.getUChar("boot_brightness", BOOT_BRIGHTNESS_LATEST);
    if (saved_boot_brightness > BOOT_BRIGHTNESS_HIGHEST)
        saved_boot_brightness = BOOT_BRIGHTNESS_LATEST;
    g_boot_brightness = (BootBrightness)saved_boot_brightness;
    g_boot_floppy_emulator = preferences.getBool("floppy_emulator", true);
    g_night_mode.enabled = preferences.getBool("night_enabled", false);
    g_night_mode.start_hour = preferences.getUChar("night_start", 22);
    g_night_mode.end_hour = preferences.getUChar("night_end", 7);
    g_night_mode.screen_off_enabled =
        preferences.getBool("night_off", false);
    g_night_mode.screen_off_hour =
        preferences.getUChar("night_off_at", 23);
    if (g_night_mode.start_hour >= 24)
        g_night_mode.start_hour = 22;
    if (g_night_mode.end_hour >= 24)
        g_night_mode.end_hour = 7;
    if (g_night_mode.screen_off_hour >= 24)
        g_night_mode.screen_off_hour = 23;
    const uint8_t saved_chime_mode =
        preferences.getUChar("chime_mode", CHIME_MODE_OFF);
    g_chime.mode =
        saved_chime_mode < CHIME_MODE_COUNT
            ? (ChimeMode)saved_chime_mode
            : CHIME_MODE_OFF;
    g_chime.sound = preferences.getUChar("chime_sound", 0);
    g_chime.volume = preferences.getUChar("chime_volume", 1);
    g_chime.quiet_enabled =
        preferences.getBool("chime_quiet", true);
    g_chime.quiet_start_hour =
        preferences.getUChar("quiet_start", 22);
    g_chime.quiet_end_hour =
        preferences.getUChar("quiet_end", 7);
    if (g_chime.sound >=
        sizeof(g_legacy_chime_sound_paths) /
            sizeof(g_legacy_chime_sound_paths[0]))
    {
        g_chime.sound = 0;
    }
    const String saved_chime_path = preferences.getString(
        "chime_path",
        g_legacy_chime_sound_paths[g_chime.sound]);
    strlcpy(
        g_chime_sound_path, saved_chime_path.c_str(),
        sizeof(g_chime_sound_path));
    if (g_chime.volume >=
        sizeof(g_chime_volumes) / sizeof(g_chime_volumes[0]))
    {
        g_chime.volume = 1;
    }
    if (g_chime.quiet_start_hour >= 24)
        g_chime.quiet_start_hour = 22;
    if (g_chime.quiet_end_hour >= 24)
        g_chime.quiet_end_hour = 7;

    heap_caps_malloc_extmem_enable(0);
    LittleFS.begin();
    sound_selector_scan();
    my_lcd.init();
    touch_eeprom_begin();
    touch.begin();

    my_lcd.setAddrWindow(0, 0, LCD_W, LCD_H);
    my_lcd.fillScreen(TFT_BLACK);
    my_lcd.setRotation(3);

    // LVGL renders a 304x224 viewport, not the full 320x240 panel.
    touch_init(my_lcd.width() - 16, my_lcd.height() - 16,
               my_lcd.getRotation());
    touch_load_calibration();

    pinMode(GPIO_FLOPPY, INPUT);
    pinMode(GPIO_ALARM, INPUT);
    pinMode(GPIO_CLOCK, INPUT);
    pinMode(GPIO_ENCODER1, INPUT_PULLUP);
    pinMode(GPIO_ENCODER2, INPUT_PULLUP);
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachHalfQuad(GPIO_ENCODER1, GPIO_ENCODER2);
    apply_boot_brightness(g_boot_brightness, false);

    const bool boot_options_requested = !digitalRead(GPIO_CLOCK);
    bool emulator_returned_to_menu = false;

    if (!boot_options_requested && g_boot_floppy_emulator) {
        run_emulator();
        emulator_returned_to_menu = true;
    }

    setup_codec();
    sound_selector_set_preview_callback(start_mp3_playback);
    setup_lvgl_display();
    setup_lvgl_input();
    lvgl_fs_init_littlefs();
    init_ui_assets();
    Wire.begin(I2C_SDA, I2C_SCL);
    setup_rtc();
    {
        char rtc_status[64];
        format_rtc_health(rtc_status, sizeof(rtc_status));
        Serial.println(rtc_status);
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
        &g_input_task_handle,
        1);

    xTaskCreatePinnedToCore(
        audio_task,
        "audio_task",
        4096,
        nullptr,
        2,
        &g_audio_task_handle,
        0);
    setup_weather_sensor();

    if (boot_options_requested || emulator_returned_to_menu)
        request_state(UI_STATE_BOOT_OPTIONS);
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
    static uint32_t calib_sum_x = 0;
    static uint32_t calib_sum_y = 0;
    static uint16_t calib_sample_count = 0;
    static lv_point_t calib_targets[4] = {};
    static bool boot_options_clock_armed = false;
    static unsigned long last_alarm_check_ms = 0;
    static unsigned long last_encoder_save_ms = 0;
    static unsigned long full_brightness_until = 0;
    static unsigned long last_night_check_ms = 0;
    static int screensaver_last_encoder = INT32_MIN;
    static NightDisplayState scheduled_display_state =
        NIGHT_DISPLAY_NORMAL;

    unsigned long now = millis();
    InputState inputs = read_input_state();
    const bool screen_touch_pressed =
        touch_consume_press_edge();
    const int observed_encoder = encoder.getCount();
    const bool rotary_activity =
        screensaver_last_encoder != INT32_MIN &&
        observed_encoder != screensaver_last_encoder;
    screensaver_last_encoder = observed_encoder;
    timer_update(now);

    if (g_requested_state != 0)
    {
        currentState = g_requested_state;
        stateStartTime = now;
        g_requested_state = 0;
    }

    if (lastState == UI_STATE_WIFI_SETUP &&
        currentState != UI_STATE_WIFI_SETUP)
    {
        wifi_mode_stop_portal();
    }

    uint32_t synchronized_epoch = 0;
    if (wifi_mode_take_time_sync(synchronized_epoch))
    {
        rtc_adjust_datetime(DateTime(synchronized_epoch));
        const DateTime synchronized = rtc_now();
        Serial.printf(
            "RTC synchronized by NTP: %04d-%02d-%02d %02d:%02d:%02d\n",
            synchronized.year(), synchronized.month(), synchronized.day(),
            synchronized.hour(), synchronized.minute(),
            synchronized.second());
    }

    if ((currentState == UI_STATE_NORMAL ||
         currentState == UI_STATE_SET_DATETIME ||
         currentState == UI_STATE_ALARM_EDITOR ||
         currentState == UI_STATE_TIMER_EDITOR) &&
        (!last_alarm_check_ms || now - last_alarm_check_ms >= 250))
    {
        last_alarm_check_ms = now;
        const int due_alarm = alarms_due(rtc_now());
        if (due_alarm >= 0)
        {
            g_active_alarm_index = due_alarm;
            currentState = UI_STATE_ALARM_RINGING;
            stateStartTime = now;
        }
    }

    if (currentState != UI_STATE_ALARM_RINGING &&
        currentState != UI_STATE_TIMER_FINISHED &&
        timer_take_finished())
    {
        currentState = UI_STATE_TIMER_FINISHED;
        stateStartTime = now;
    }

    if (!last_night_check_ms || now - last_night_check_ms >= 1000)
    {
        const DateTime current = rtc_now();
        scheduled_display_state = night_display_state(current);
        if (currentState == UI_STATE_NORMAL ||
            currentState == UI_STATE_SET_DATETIME ||
            currentState == UI_STATE_ALARM_EDITOR ||
            currentState == UI_STATE_TIMER_EDITOR)
        {
            maybe_start_chime(current);
        }
        last_night_check_ms = now;
    }

    const bool temporary_wake_active =
        (int32_t)(full_brightness_until - now) > 0;
    if (currentState == UI_STATE_NORMAL &&
        !g_screensaver_active &&
        scheduled_display_state != NIGHT_DISPLAY_NORMAL &&
        !temporary_wake_active &&
        (inputs.clock || inputs.alarm))
    {
        full_brightness_until = now + 10000;
        inputs.clock = false;
        inputs.alarm = false;
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
            start_mp3_playback("/startup.mp3", 80);
            wifi_mode_start_task();
            g_requested_state = currentState + 1;
            stateStartTime = now;
        }
        break;
    case UI_STATE_WAIT_STARTUP_SOUND: // wait for end of startup sound
    {
        if (consume_mp3_finished())
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
        start_mp3_playback("/floppy.mp3", 65);
    }
        g_requested_state = currentState + 1;
        stateStartTime = now;
        break;
    case UI_STATE_BOOT_PLUGINS: // show boot screen + detected i2c plugins
        if (currentState != lastState)
        {
            const uint8_t k_addrs[k_plugin_max] = {0x18, 0x38, g_weather_sensor_address, 0x68};
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
                if (addr != 0 && i2c_device_present(addr) && plugin_count < k_plugin_max)
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
        if (consume_mp3_finished())
        {
            g_requested_state = currentState + 1;
            stateStartTime = now;
        }
    }
    break;
    case UI_STATE_NORMAL: // normal state
    {
        static unsigned long lastClockUpdate = 0;
        static unsigned long dual_key_hold_start = 0;
        static bool dual_key_handled = false;
        static bool clock_press_pending = false;
        static bool alarm_press_pending = false;
        static constexpr unsigned long kDualKeyHoldMs = 2000;
        if (currentState != lastState)
        {
            wifi_mode_start_task();
            lastClockUpdate = 0;
            dual_key_hold_start = 0;
            dual_key_handled = false;
            clock_press_pending = false;
            alarm_press_pending = false;
            g_last_clock_activity_ms = now;
            screensaver_last_encoder = observed_encoder;
            show_selected_clock_face();
            update_selected_clock_face(now);
            lv_timer_handler();
        }

        const bool clock_activity =
            inputs.clock || inputs.alarm || inputs.touch ||
            screen_touch_pressed || rotary_activity;
        if (g_screensaver_active)
        {
            if (clock_activity)
            {
                g_last_clock_activity_ms = now;
                full_brightness_until = now + 10000;
                inputs.clock = false;
                inputs.alarm = false;
                inputs.touch = false;
                dual_key_hold_start = 0;
                dual_key_handled = false;
                clock_press_pending = false;
                alarm_press_pending = false;
                show_selected_clock_face();
                update_selected_clock_face(now);
            }
            else
            {
                update_after_dark_screensaver();
            }
            lv_timer_handler();
            break;
        }

        if (clock_activity)
            g_last_clock_activity_ms = now;
        const unsigned long screensaver_delay_ms =
            (unsigned long)
                g_screensaver_delays_minutes[
                    g_screensaver_delay_index] *
            60000UL;
        if (g_screensaver_mode == SCREENSAVER_AFTER_DARK &&
            now - g_last_clock_activity_ms >=
                screensaver_delay_ms)
        {
            show_after_dark_screensaver();
            update_after_dark_screensaver();
            lv_timer_handler();
            break;
        }

        if (!lastClockUpdate || now - lastClockUpdate >= 100)
        {
            if (g_clock_face == CLOCK_FACE_MACINTOSH &&
                inputs.floppy)
                lv_obj_clear_flag(g_ui.icon, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(g_ui.icon, LV_OBJ_FLAG_HIDDEN);
            update_selected_clock_face(now);
            lv_timer_handler();
            lastClockUpdate = now;
        }

        const bool clock_button_down = digitalRead(GPIO_CLOCK) == LOW;
        const bool alarm_button_down = digitalRead(GPIO_ALARM) == LOW;
        if (clock_button_down && alarm_button_down)
        {
            if (!dual_key_hold_start)
                dual_key_hold_start = now;
            else if (!dual_key_handled &&
                     now - dual_key_hold_start >= kDualKeyHoldMs)
            {
                dual_key_handled = true;
                clock_press_pending = false;
                alarm_press_pending = false;
                g_requested_state = UI_STATE_BOOT_OPTIONS;
                stateStartTime = now;
            }
        }
        else
        {
            dual_key_hold_start = 0;
            if (!clock_button_down && !alarm_button_down)
                dual_key_handled = false;
        }

        if (inputs.clock && !dual_key_handled)
            clock_press_pending = true;
        if (inputs.alarm && !dual_key_handled)
            alarm_press_pending = true;

        if (dual_key_handled)
        {
            clock_press_pending = false;
            alarm_press_pending = false;
        }
        else if (clock_press_pending && !clock_button_down)
        {
            clock_press_pending = false;
            alarm_press_pending = false;
            g_requested_state = UI_STATE_SET_DATETIME;
            stateStartTime = now;
        }
        else if (alarm_press_pending && !alarm_button_down)
        {
            clock_press_pending = false;
            alarm_press_pending = false;
            g_requested_state = UI_STATE_ALARM_EDITOR;
            stateStartTime = now;
        }
        break;
    }
    case UI_STATE_SET_DATETIME: // change date/time
        if (currentState != lastState)
        {
            DateTime current = rtc_now();
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
    case UI_STATE_ALARM_EDITOR:
        if (currentState != lastState)
            alarm_ui_enter();
        hide_all_ui();
        lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.white_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.black_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
        alarm_ui_show_editor();
        lv_timer_handler();
        break;
    case UI_STATE_ALARM_RINGING:
        if (currentState != lastState)
        {
            hide_all_ui();
            lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.white_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.black_line, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
            alarm_ui_show_ringing((size_t)g_active_alarm_index);
            start_mp3_playback(
                alarms_sound_path((size_t)g_active_alarm_index),
                alarms_volume((size_t)g_active_alarm_index));
        }
        lv_timer_handler();
        if (inputs.alarm || inputs.touch)
            alarm_snooze_current();
        else if (inputs.clock)
            alarm_dismiss_current();
        else if (consume_mp3_finished() && g_active_alarm_index >= 0)
        {
            start_mp3_playback(
                alarms_sound_path((size_t)g_active_alarm_index),
                alarms_volume((size_t)g_active_alarm_index));
        }
        break;
    case UI_STATE_TIMER_EDITOR:
        if (currentState != lastState)
            timer_ui_enter(now);
        hide_all_ui();
        lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.white_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.black_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
        timer_ui_show(now);
        lv_timer_handler();
        break;
    case UI_STATE_TIMER_FINISHED:
        if (currentState != lastState)
        {
            hide_all_ui();
            lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.white_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.black_line, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
            timer_ui_show_finished();
            start_mp3_playback(timer_sound_path(), timer_volume());
        }
        lv_timer_handler();
        if (inputs.clock || inputs.alarm)
            timer_dismiss_current();
        else if (consume_mp3_finished())
            start_mp3_playback(timer_sound_path(), timer_volume());
        break;
    case UI_STATE_BOOT_OPTIONS:
        if (currentState != lastState)
        {
            boot_options_clock_armed = false;
            hide_all_ui();
            lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
            show_boot_options_ui();
        }
        lv_timer_handler();
        if (!boot_options_clock_armed)
        {
            if (digitalRead(GPIO_CLOCK))
                boot_options_clock_armed = true;
        }
        else if (inputs.clock)
        {
            g_requested_state = UI_STATE_CALIBRATION;
            stateStartTime = now;
        }
        break;
    case UI_STATE_EMULATOR:
        run_emulator();
        g_requested_state = UI_STATE_BOOT_OPTIONS;
        stateStartTime = now;
        break;
    case UI_STATE_DIAGNOSTICS:
    {
        static unsigned long last_diagnostics_update = 0;
        if (currentState != lastState)
        {
            hide_all_ui();
            lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_diagnostics_ui.panel, LV_OBJ_FLAG_HIDDEN);
            last_diagnostics_update = 0;
        }
        if (!last_diagnostics_update ||
            now - last_diagnostics_update >= 250)
        {
            update_diagnostics_ui();
            lv_timer_handler();
            last_diagnostics_update = now;
        }
        break;
    }
    case UI_STATE_WIFI_SETUP:
    {
        static unsigned long last_wifi_setup_update = 0;
        if (currentState != lastState)
        {
            hide_all_ui();
            lv_obj_clear_flag(g_ui.background, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_ui.corners, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(
                g_wifi_setup_ui.panel, LV_OBJ_FLAG_HIDDEN);
            wifi_mode_start_portal();
            last_wifi_setup_update = 0;
        }
        wifi_mode_process_portal();
        if (!last_wifi_setup_update ||
            now - last_wifi_setup_update >= 500)
        {
            const WifiModeSnapshot wifi = wifi_mode_snapshot();
            char setup_status[180];
            snprintf(
                setup_status, sizeof(setup_status),
                tr("1. Connect to Wi-Fi:\nMaclock Setup\n\n"
                   "2. Open 192.168.4.1\n\n%s"),
                tr(wifi.status));
            lv_label_set_text(
                g_wifi_setup_ui.status, setup_status);
            lv_timer_handler();
            last_wifi_setup_update = now;
        }
        break;
    }
    case UI_STATE_CALIBRATION: // calibration screen
        if (inputs.clock)
        {
            g_requested_state = UI_STATE_BOOT_OPTIONS;
            stateStartTime = now;
            break;
        }
        if (currentState != lastState)
        {
            lv_obj_t *scr = lv_screen_active();
            int w = lv_obj_get_width(scr);
            int h = lv_obj_get_height(scr);
            int margin = 16;
            calib_targets[0] = {margin, margin};
            calib_targets[1] = {w - 1 - margin, margin};
            calib_targets[2] = {w - 1 - margin, h - 1 - margin};
            calib_targets[3] = {margin, h - 1 - margin};
            calib_step = 0;
            calib_wait_release = false;
            calib_sum_x = 0;
            calib_sum_y = 0;
            calib_sample_count = 0;
            if (g_cursor)
                lv_obj_add_flag(g_cursor, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(g_calib_ui.label, tr("Touch the crosshair"));
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
                calib_wait_release = true;
                calib_sum_x = 0;
                calib_sum_y = 0;
                calib_sample_count = 0;
            }
            if (touched && calib_wait_release && calib_sample_count < 64)
            {
                calib_sum_x += raw_x;
                calib_sum_y += raw_y;
                calib_sample_count++;
            }
            if (!touched && calib_wait_release)
            {
                calib_wait_release = false;
                if (calib_sample_count == 0)
                    break;
                calib_raw_x[calib_step] =
                    (calib_sum_x + calib_sample_count / 2) /
                    calib_sample_count;
                calib_raw_y[calib_step] =
                    (calib_sum_y + calib_sample_count / 2) /
                    calib_sample_count;
                calib_step++;
                if (calib_step < 4)
                {
                    calib_set_cross_pos(calib_targets[calib_step]);
                }
                else
                {
                    lv_obj_t *scr = lv_screen_active();
                    int w = lv_obj_get_width(scr);
                    int h = lv_obj_get_height(scr);
                    uint16_t minx = 0;
                    uint16_t maxx = 0;
                    uint16_t miny = 0;
                    uint16_t maxy = 0;
                    bool valid =
                        calib_axis_bounds(
                            calib_raw_x[0], calib_raw_x[3],
                            calib_raw_x[1], calib_raw_x[2],
                            calib_targets[0].x, calib_targets[1].x,
                            w, minx, maxx) &&
                        calib_axis_bounds(
                            calib_raw_y[0], calib_raw_y[1],
                            calib_raw_y[3], calib_raw_y[2],
                            calib_targets[0].y, calib_targets[3].y,
                            h, miny, maxy);
                    if (valid)
                    {
                        touch_set_calibration(minx, maxx, miny, maxy);
                        touch_save_calibration();
                        g_requested_state = UI_STATE_NORMAL;
                        stateStartTime = now;
                    }
                    else
                    {
                        calib_step = 0;
                        lv_label_set_text(
                            g_calib_ui.label,
                            tr("Calibration failed - try again"));
                        calib_set_cross_pos(calib_targets[0]);
                    }
                }
            }
        }
        break;
    }

    lastState = currentState;

    if (inputs.touch ||
        (screen_touch_pressed &&
         currentState == UI_STATE_NORMAL &&
         scheduled_display_state != NIGHT_DISPLAY_NORMAL))
        full_brightness_until = now + 10000;

    int enc = encoder.getCount();
    if (enc < 0)
        enc = 0;
    if (enc > kBrightnessMax)
        enc = kBrightnessMax;
    if (enc != encoder.getCount())
        encoder.setCount(enc);
    if ((int32_t)(full_brightness_until - now) > 0)
        analogWrite(TFT_BL_VAR, 255);
    else if (currentState == UI_STATE_NORMAL &&
             scheduled_display_state == NIGHT_DISPLAY_OFF)
        analogWrite(TFT_BL_VAR, 0);
    else if (currentState == UI_STATE_NORMAL &&
             scheduled_display_state == NIGHT_DISPLAY_DIMMED)
        analogWrite(
            TFT_BL_VAR,
            brightness_to_pwm(min(enc, 1)));
    else
        analogWrite(TFT_BL_VAR, brightness_to_pwm(enc));

    if (enc != g_last_saved_encoder && (now - last_encoder_save_ms) >= 500)
    {
        preferences.putUChar("brightness", (uint8_t)enc);
        g_last_saved_encoder = enc;
        last_encoder_save_ms = now;
    }
}

