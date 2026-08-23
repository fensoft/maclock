#define MACLOCK_COMBINED_SOURCE

#include <lvgl.h>
#include "maclock_app.h"
#include "src/draw/lv_image_decoder_private.h"
#include "AudioFileSourceLittleFS.h"
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
#include <ArduinoJson.h>
#include <math.h>
#include "datetime_ui.h"
#include "alarm_ui.h"
#include "timer_ui.h"
#include "touch.h"
#include <Preferences.h>
#include "brightness.h"
#include "localization.h"
#include "regional_settings.h"
#include "selector_list_style.h"
#include "sound_selector.h"
#include "wifi_mode.h"
#include "i2c_bus.h"
#include "rtc_service.h"
#include "weather_service.h"
#include "input_service.h"
#include "audio_service.h"
#include "control_panel.h"
#include "mqtt_service.h"
#include "settings_store.h"
#include "fensoft_logo.h"

LV_FONT_DECLARE(lv_font_chicago_8);
LV_FONT_DECLARE(lv_font_chicago_24);
LV_FONT_DECLARE(lv_font_chicago_32);
LV_FONT_DECLARE(lv_font_chicago_48);
LV_FONT_DECLARE(lv_font_chicago_digits_6);
LV_FONT_DECLARE(lv_font_chicago_digits_10);
LV_FONT_DECLARE(lv_font_chicago_digits_40);
LV_FONT_DECLARE(lv_font_chicago_digits_56);
LV_FONT_DECLARE(lv_font_seven_segment_24);
LV_FONT_DECLARE(lv_font_seven_segment_48);
LV_FONT_DECLARE(lv_font_seven_segment_64);
LV_FONT_DECLARE(lv_font_seven_segment_80);
LV_FONT_DECLARE(lv_font_seven_segment_96);

static MaclockApp *active_app = nullptr;

#define settings_store (active_app->settingsStore())
#define app_settings (active_app->settings())
#define i2c_bus (active_app->i2cBus())
#define rtc_service (active_app->rtc())
#define weather_service (active_app->weather())
#define input_service (active_app->input())
#define display_service (active_app->display())
#define audio_service (active_app->audio())
#define wifi_service (active_app->wifi())
#define control_panel_service (active_app->controlPanel())
#define mqtt_service (active_app->mqtt())
#define update_service (active_app->updates())
#define alarm_service (active_app->alarms())
#define alarm_view (active_app->alarmView())
#define timer_service (active_app->timer())
#define timer_view (active_app->timerView())
#define datetime_editor (active_app->dateTimeEditor())

static constexpr size_t k_plugin_max = 4;

class UiShell
{
public:
    void init();
    void hideAll();
    void updateMenuTitles();
    void updateBootMessage();
    void applyMacintoshAppearance(bool macos8);
    void releaseMacOS8Assets();

    lv_obj_t *background;
    lv_obj_t *corners;
    lv_obj_t *disk_missing_1;
    lv_obj_t *disk_missing_2;
    lv_obj_t *boot;
    lv_obj_t *boot_message;
    lv_obj_t *boot_progress;
    lv_obj_t *menu;
    lv_obj_t *menu_titles;
    lv_obj_t *menu_right;
    lv_obj_t *icon;
    lv_obj_t *clock;
    lv_obj_t *clock_label;
    lv_obj_t *time;
    lv_obj_t *time_meridiem;
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
    lv_draw_buf_t *macos8_background_buf;
    lv_draw_buf_t *macos8_corners_buf;
    lv_draw_buf_t *macos8_menu_buf;
    lv_draw_buf_t *macos8_floppy_buf;
    lv_draw_buf_t *macos8_clock_buf;
    lv_draw_buf_t *macos8_alarm_buf;
    bool macos8_assets_loaded;
};

class CalibrationView
{
public:
    lv_obj_t *label;
    lv_obj_t *cross;
    int step = 0;
    bool wait_release = false;
    uint16_t raw_x[4] = {};
    uint16_t raw_y[4] = {};
    uint32_t sum_x = 0;
    uint32_t sum_y = 0;
    uint16_t sample_count = 0;
    lv_point_t targets[4] = {};
};

class StartupView
{
public:
    uint8_t plugin_count = 0;
    uint8_t plugin_reveal = 0;
    unsigned long next_reveal_ms = 0;
    lv_draw_buf_t *plugin_buffers[k_plugin_max] = {};
};

enum BootOptionsPage
{
    BOOT_OPTIONS_HOME,
    BOOT_OPTIONS_LANGUAGE,
    BOOT_OPTIONS_REGIONAL,
    BOOT_OPTIONS_DATETIME,
    BOOT_OPTIONS_CLOCK_FACE,
    BOOT_OPTIONS_FACE_SETTINGS,
    BOOT_OPTIONS_SCREENSAVER,
    BOOT_OPTIONS_NIGHT_SCHEDULE,
    BOOT_OPTIONS_NIGHT_SCREEN,
    BOOT_OPTIONS_CHIME,
    BOOT_OPTIONS_CHIME_SOUND,
    BOOT_OPTIONS_CHIME_VOLUME,
    BOOT_OPTIONS_CHIME_QUIET,
    BOOT_OPTIONS_START,
    BOOT_OPTIONS_PREFERENCES,
    BOOT_OPTIONS_WIFI,
    BOOT_OPTIONS_TOOLS,
    BOOT_OPTIONS_UPDATE,
    BOOT_OPTIONS_ABOUT,
    BOOT_OPTIONS_PAGE_COUNT
};

enum BootOptionsSection : uint8_t
{
    BOOT_OPTIONS_SECTION_GENERAL,
    BOOT_OPTIONS_SECTION_DISPLAY,
    BOOT_OPTIONS_SECTION_SOUND,
    BOOT_OPTIONS_SECTION_SYSTEM,
    BOOT_OPTIONS_SECTION_COUNT
};

enum BootDateTimeField : uint8_t
{
    BOOT_DATETIME_HOUR,
    BOOT_DATETIME_MINUTE,
    BOOT_DATETIME_SECOND,
    BOOT_DATETIME_DAY,
    BOOT_DATETIME_MONTH,
    BOOT_DATETIME_YEAR,
    BOOT_DATETIME_FIELD_COUNT
};

static constexpr size_t kFilesystemClockFaceMax = 32;

class BootOptionsView
{
public:
    void init(lv_obj_t *screen);
    void show();
    void setPage(BootOptionsPage page);
    void refreshDateTime();
    void refreshUpdate();
    void tick(uint32_t now);
    BootOptionsPage page = BOOT_OPTIONS_HOME;
    BootOptionsPage page_on_show = BOOT_OPTIONS_HOME;
    bool standalone_update_prompt = false;
    bool clock_armed = false;

    lv_obj_t *panel;
    lv_obj_t *title_bar;
    lv_obj_t *title;
    lv_obj_t *title_close;
    lv_obj_t *title_zoom;
    lv_obj_t *pages[BOOT_OPTIONS_PAGE_COUNT];
    lv_obj_t *section_labels[BOOT_OPTIONS_SECTION_COUNT];
    lv_obj_t *home_calibration_label;
    lv_obj_t *brightness_options;
    lv_obj_t *language_options;
    lv_obj_t *language_items[UI_LANGUAGE_COUNT];
    lv_obj_t *date_format_options;
    lv_obj_t *regional_hour_button;
    lv_obj_t *regional_hour_label;
    lv_obj_t *regional_temperature_button;
    lv_obj_t *regional_temperature_label;
    lv_obj_t *regional_seconds_button;
    lv_obj_t *regional_seconds_label;
    lv_obj_t *datetime_fields;
    lv_obj_t *datetime_minus;
    lv_obj_t *datetime_plus;
    char datetime_text[BOOT_DATETIME_FIELD_COUNT][24];
    const char *datetime_map[8];
    BootDateTimeField datetime_field_order[BOOT_DATETIME_FIELD_COUNT];
    BootDateTimeField datetime_selected = BOOT_DATETIME_HOUR;
    uint32_t datetime_last_refresh_ms = 0;
    lv_obj_t *clock_face_options;
    lv_obj_t *clock_face_items[kFilesystemClockFaceMax] = {};
    char clock_face_names[kFilesystemClockFaceMax][AppSettings::kCustomClockFaceNameMax] = {};
    char clock_face_labels[kFilesystemClockFaceMax][64] = {};
    uint8_t clock_face_count = 0;
    lv_obj_t *flip_speed_label;
    lv_obj_t *flip_speed_options;
    lv_obj_t *colon_blink_label;
    lv_obj_t *colon_blink_options;
    lv_obj_t *continuous_seconds_label;
    lv_obj_t *continuous_seconds_options;
    lv_obj_t *screensaver_options;
    lv_obj_t *screensaver_items[
        static_cast<uint8_t>(ScreensaverMode::Count)];
    lv_obj_t *screensaver_delay_options;
    lv_obj_t *screensaver_delay_items[kScreensaverDelayCount];
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
    lv_obj_t *update_status;
    lv_obj_t *update_progress;
    lv_obj_t *update_primary;
    lv_obj_t *update_primary_label;
    lv_obj_t *update_later;
    lv_obj_t *update_later_label;
    lv_obj_t *update_ignore;
    lv_obj_t *update_ignore_label;
    uint32_t update_last_refresh_ms = 0;
    lv_obj_t *about_logo;
    lv_obj_t *about_qr;
    lv_obj_t *about_author;
    lv_obj_t *about_link;
    lv_obj_t *previous;
    lv_obj_t *previous_label;
    lv_obj_t *exit;
    lv_obj_t *exit_label;
    lv_obj_t *next;
    lv_obj_t *next_label;
    lv_obj_t *brightness_label;
    lv_obj_t *screensaver_delay_label;
    lv_obj_t *dim_from_label;
    lv_obj_t *normal_at_label;
    lv_obj_t *screen_off_label;
    lv_obj_t *quiet_from_label;
    lv_obj_t *quiet_end_label;
    lv_obj_t *wifi_setup_label;
    lv_obj_t *clock_button_label;
    lv_obj_t *emulator_button_label;
    lv_obj_t *boot_mode_button_label;
    lv_obj_t *diagnostics_button_label;
    lv_obj_t *calibration_label;
};


static constexpr ScreensaverMode SCREENSAVER_OFF =
    ScreensaverMode::Off;
static constexpr ScreensaverMode SCREENSAVER_AFTER_DARK =
    ScreensaverMode::AfterDark;
static constexpr uint8_t SCREENSAVER_MODE_COUNT =
    static_cast<uint8_t>(ScreensaverMode::Count);

static constexpr size_t kScreensaverStarCount = 24;
static constexpr size_t kScreensaverMatrixColumnCount = 12;
static constexpr size_t kScreensaverPipeSegmentCount = 32;
static constexpr size_t kScreensaverFlyingClockCount = 5;

struct FlipCardAnimation
{
    lv_obj_t *card;
    lv_obj_t *left_pin;
    lv_obj_t *right_pin;
    lv_obj_t *top_panel;
    lv_obj_t *top_label;
    lv_obj_t *bottom_panel;
    lv_obj_t *bottom_label;
    lv_obj_t *top_flap;
    lv_obj_t *top_flap_label;
    lv_obj_t *bottom_flap;
    lv_obj_t *bottom_flap_label;
    char displayed[3];
    char pending[3];
    bool initialized;
    bool animating;
};

enum class CustomFaceWidgetType : uint8_t
{
    None,
    Flip,
    Odometer,
    Colon
};

struct CustomFaceWidget
{
    CustomFaceWidgetType type = CustomFaceWidgetType::None;
    lv_obj_t *root = nullptr;
    lv_obj_t *current[12] = {};
    lv_obj_t *next[12] = {};
    FlipCardAnimation flip_cards[12] = {};
    const lv_font_t *font = nullptr;
    lv_color_t stroke = lv_color_white();
    lv_text_align_t align = LV_TEXT_ALIGN_CENTER;
    char template_text[64] = {};
    char displayed[64] = {};
    int16_t width = 0;
    int16_t height = 0;
    int16_t text_y = 0;
    uint8_t character_count = 0;
    bool blink = false;
};

struct CustomFaceDynamicLine
{
    lv_obj_t *object = nullptr;
    lv_point_precise_t points[2] = {};
    char angle_template[32] = {};
    float maximum = 0.0f;
    int16_t x = 0;
    int16_t y = 0;
    int16_t length = 0;
};

struct ClockRenderSnapshot
{
    DateTime current;
    WeatherReading sensor;
    WifiModeSnapshot online;
    bool timer_active;
    char timer_remaining[12];
    bool alarm_indicator;
};

struct DiagnosticsSnapshot
{
    bool clock_pressed;
    bool alarm_pressed;
    bool floppy_inserted;
    bool touch_pressed;
    bool touchscreen_present;
    bool charging;
    int64_t encoder_position;
    char i2c_devices[48];
    char rtc_status[64];
    WifiModeSnapshot wifi;
};

class ClockView
{
public:
    void init(lv_obj_t *screen);
    void show(const ClockRenderSnapshot &snapshot);
    void update(const ClockRenderSnapshot &snapshot);
    void initScreensavers(lv_obj_t *screen);
    void showScreensaver(ScreensaverMode mode);
    void updateScreensaver(
        const ClockRenderSnapshot &snapshot);
    void activateScreensaverMode(
        ScreensaverMode mode, bool reset);
    void initExtendedScreensavers(lv_obj_t *parent);
    bool activateExtendedScreensaver(
        ScreensaverMode mode, bool reset);
    bool updateExtendedScreensaver(
        const ClockRenderSnapshot &snapshot);
    bool showCustomFace(const ClockRenderSnapshot &snapshot);
    void updateCustomFace(const ClockRenderSnapshot &snapshot);
    void refreshCustomFaceWidgets(const ClockRenderSnapshot &snapshot);
    void refreshCustomFaceLines(const ClockRenderSnapshot &snapshot);
    void hideCustomFace();

    lv_obj_t *custom_face;
    lv_obj_t *custom_error;
    char custom_loaded_name[AppSettings::kCustomClockFaceNameMax];
    char custom_image_paths[64][96];
    lv_point_precise_t custom_line_points[64][2];
    CustomFaceWidget custom_widgets[8];
    size_t custom_widget_count = 0;
    CustomFaceDynamicLine custom_lines[32];
    size_t custom_line_count = 0;
    bool custom_loaded = false;
    uint32_t custom_last_refresh_ms = 0;
    lv_obj_t *screensaver;
    lv_obj_t *screensaver_star_layer;
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
    lv_obj_t *screensaver_logo_layer;
    lv_obj_t *screensaver_logo;
    int16_t screensaver_logo_x;
    int16_t screensaver_logo_y;
    int8_t screensaver_logo_dx;
    int8_t screensaver_logo_dy;
    lv_obj_t *screensaver_matrix_layer;
    lv_obj_t *screensaver_matrix_columns[
        kScreensaverMatrixColumnCount];
    char screensaver_matrix_text[
        kScreensaverMatrixColumnCount][24];
    int16_t screensaver_matrix_y[
        kScreensaverMatrixColumnCount];
    uint8_t screensaver_matrix_speed[
        kScreensaverMatrixColumnCount];
    lv_obj_t *screensaver_pipe_layer;
    lv_obj_t *screensaver_pipe_segments[
        kScreensaverPipeSegmentCount];
    uint8_t screensaver_pipe_index;
    int16_t screensaver_pipe_x;
    int16_t screensaver_pipe_y;
    int8_t screensaver_pipe_direction;
    uint8_t screensaver_pipe_color;
    lv_obj_t *screensaver_flying_layer;
    lv_obj_t *screensaver_flying_clocks[
        kScreensaverFlyingClockCount];
    lv_obj_t *screensaver_flying_times[
        kScreensaverFlyingClockCount];
    int16_t screensaver_flying_x[
        kScreensaverFlyingClockCount];
    int16_t screensaver_flying_y[
        kScreensaverFlyingClockCount];
    int8_t screensaver_flying_dx[
        kScreensaverFlyingClockCount];
    int8_t screensaver_flying_dy[
        kScreensaverFlyingClockCount];
    ScreensaverMode screensaver_active_mode =
        ScreensaverMode::Off;
    unsigned long screensaver_random_due_ms = 0;
    int last_second = -1;
    int16_t gauge_width = 0;
    int16_t gauge_box_width = 0;
    unsigned long screensaver_last_move_ms = 0;
    int screensaver_last_second = -1;
    bool screensaver_active = false;
    unsigned long last_activity_ms = 0;
    unsigned long last_update_ms = 0;
    ClockRenderSnapshot screensaver_snapshot{};
    unsigned long screensaver_snapshot_ms = 0;
    lv_obj_t *screensaver_canvas = nullptr;
    lv_obj_t *screensaver_photo = nullptr;
    uint8_t *screensaver_canvas_buffer = nullptr;
    lv_draw_buf_t *screensaver_toaster_atlas = nullptr;
    lv_draw_buf_t *screensaver_fish_atlas = nullptr;
    lv_draw_buf_t *screensaver_error_atlas = nullptr;
    alignas(2) uint8_t screensaver_state[2200] = {};
    int16_t screensaver_x[64] = {};
    int16_t screensaver_y[64] = {};
    int8_t screensaver_dx[64] = {};
    int8_t screensaver_dy[64] = {};
    uint32_t screensaver_extended_frame = 0;
    uint32_t screensaver_photo_due_ms = 0;
    uint8_t screensaver_photo_transition = 0;
    bool screensaver_photo_loaded = false;
    char screensaver_photo_path[96] = "";
};

class DiagnosticsView
{
public:
    void init(lv_obj_t *screen);
    void update(const DiagnosticsSnapshot &snapshot);

    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *status;
    lv_obj_t *back_label;
    unsigned long last_update_ms = 0;
};

class WifiSetupView
{
public:
    void init(lv_obj_t *screen);

    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *qr;
    lv_obj_t *status;
    lv_obj_t *back_label;
    unsigned long last_update_ms = 0;
};

class MqttNotificationView
{
public:
    void init(lv_obj_t *screen);
    void show(const MqttMessage &message);
    void hide();

    lv_obj_t *panel = nullptr;
    lv_obj_t *title = nullptr;
    lv_obj_t *message = nullptr;
    lv_obj_t *ok_button = nullptr;
};

static constexpr BootBrightness BOOT_BRIGHTNESS_LATEST =
    BootBrightness::Latest;
static constexpr BootBrightness BOOT_BRIGHTNESS_LOWEST =
    BootBrightness::Lowest;
static constexpr BootBrightness BOOT_BRIGHTNESS_HIGHEST =
    BootBrightness::Highest;

static constexpr NightDisplayState NIGHT_DISPLAY_NORMAL =
    NightDisplayState::Normal;
static constexpr NightDisplayState NIGHT_DISPLAY_DIMMED =
    NightDisplayState::Dimmed;
static constexpr NightDisplayState NIGHT_DISPLAY_OFF =
    NightDisplayState::Off;

static constexpr ChimeMode CHIME_MODE_OFF = ChimeMode::Off;
static constexpr ChimeMode CHIME_MODE_HOURLY =
    ChimeMode::Hourly;
static constexpr ChimeMode CHIME_MODE_QUARTER_HOUR =
    ChimeMode::QuarterHour;
static constexpr uint8_t CHIME_MODE_COUNT =
    static_cast<uint8_t>(ChimeMode::Count);

static constexpr UiState UI_STATE_EMPTY_SCREEN =
    UiState::EmptyScreen;
static constexpr UiState UI_STATE_WAIT_STARTUP_SOUND =
    UiState::WaitStartupSound;
static constexpr UiState UI_STATE_WAIT_FLOPPY_1 =
    UiState::WaitFloppy1;
static constexpr UiState UI_STATE_WAIT_FLOPPY_2 =
    UiState::WaitFloppy2;
static constexpr UiState UI_STATE_FLOPPY_INSERTED =
    UiState::FloppyInserted;
static constexpr UiState UI_STATE_BOOT_PLUGINS =
    UiState::BootPlugins;
static constexpr UiState UI_STATE_WAIT_FLOPPY_SOUND =
    UiState::WaitFloppySound;
static constexpr UiState UI_STATE_NORMAL = UiState::Normal;
static constexpr UiState UI_STATE_SET_DATETIME =
    UiState::SetDateTime;
static constexpr UiState UI_STATE_CALIBRATION =
    UiState::Calibration;
static constexpr UiState UI_STATE_BOOT_OPTIONS =
    UiState::BootOptions;
static constexpr UiState UI_STATE_EMULATOR = UiState::Emulator;
static constexpr UiState UI_STATE_DIAGNOSTICS =
    UiState::Diagnostics;
static constexpr UiState UI_STATE_ALARM_EDITOR =
    UiState::AlarmEditor;
static constexpr UiState UI_STATE_ALARM_RINGING =
    UiState::AlarmRinging;
static constexpr UiState UI_STATE_TIMER_EDITOR =
    UiState::TimerEditor;
static constexpr UiState UI_STATE_TIMER_FINISHED =
    UiState::TimerFinished;
static constexpr UiState UI_STATE_WIFI_SETUP =
    UiState::WifiSetup;

static UiState advance_state(UiState state, uint8_t count = 1)
{
    return static_cast<UiState>(
        static_cast<uint8_t>(state) + count);
}

static UiShell ui_shell = {};
static CalibrationView calibration_view = {};
static StartupView startup_view = {};
static BootOptionsView boot_options_view = {};
static ClockView clock_view = {};
static DiagnosticsView diagnostics_view = {};
static WifiSetupView wifi_setup_view = {};
static MqttNotificationView mqtt_notification_view = {};
#define app_events (*active_app)
static lv_obj_t *g_cursor = nullptr;
static lv_timer_t *g_cursor_timer = nullptr;
#define g_boot_brightness (app_settings.boot_brightness)
#define g_date_format (app_settings.date_format)
#define g_temperature_unit (app_settings.temperature_unit)
#define g_face_customization \
    (app_settings.face_customization)
#define g_time_format (app_settings.time_format)
#define g_screensaver_mode (app_settings.screensaver_mode)
#define g_screensaver_delay_index \
    (app_settings.screensaver_delay_index)
#define g_boot_floppy_emulator \
    (app_settings.boot_floppy_emulator)
#define g_night_mode (app_settings.night_mode)
#define g_chime (app_settings.chime)
static int g_last_saved_encoder = -1;
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
static char g_startup_sound_path[SOUND_SELECTOR_PATH_MAX] =
    "/startup.mp3";
static uint8_t g_startup_sound_volume = 80;
static char g_floppy_sound_path[SOUND_SELECTOR_PATH_MAX] =
    "/floppy.mp3";
static uint8_t g_floppy_sound_volume = 60;
static const char *g_brightness_map[4] = {};
static const char *g_date_format_map[4] = {
    "DD/MM/YYYY", "MM/DD/YYYY", "YYYY-MM-DD", ""};
static const uint8_t g_screensaver_delays_minutes[] = {
    1, 5, 10, 30};
static const char *g_screensaver_delay_map[7] = {};
static const char *g_night_enabled_map[3] = {};
static const char *g_night_screen_map[3] = {};
static const char *g_chime_mode_map[6] = {};
static const char *g_chime_volume_map[] = {
    "10%", "20%", "40%", "\n",
    "60%", "80%", "100%", ""};
static const char *g_chime_quiet_map[3] = {};
static const char *g_wifi_enabled_map[3] = {};

void minivmac();
static void run_emulator();
static void update_wifi_options_ui();
static void refresh_language_ui();
static bool format_rtc_health(
    char *text, size_t text_size);

static void request_state(UiState state)
{
    active_app->requestState(state);
}

MaclockApp::MaclockApp(MaclockHal &hal)
    : hal_(hal),
      rtc_service_(i2c_bus_),
      weather_service_(i2c_bus_),
      audio_service_(display_service_),
      timer_view_(timer_service_)
{
    maclock_install_hal(hal_);
}

void MaclockApp::requestState(UiState state)
{
    if (!touchscreen_available_ &&
        (state == UiState::Emulator ||
         state == UiState::Calibration))
    {
        Serial.println(
            state == UiState::Emulator
                ? "Emulator unavailable: touchscreen required"
                : "Calibration unavailable: touchscreen required");
        return;
    }
    requested_state_ = state;
}

void MaclockApp::syncUpdatePrompt(bool allowed)
{
    if (!allowed || !update_service_.consumePrompt())
        return;

    boot_options_view.standalone_update_prompt = true;
    boot_options_view.page_on_show =
        BOOT_OPTIONS_UPDATE;
    requestState(UI_STATE_BOOT_OPTIONS);
}

void MaclockApp::adjustRtc(const DateTime &date_time)
{
    rtc_service.adjust(date_time);
}

DateTime rtc_now()
{
    return rtc_service.now();
}

static ClockRenderSnapshot make_clock_snapshot(
    unsigned long now_ms)
{
    ClockRenderSnapshot snapshot = {
        rtc_now(),
        weather_service.read(),
        wifi_service.snapshot(),
        timer_service.active(),
        {},
        alarm_service.hasActiveIndicator()};
    if (snapshot.timer_active)
    {
        timer_service.formatRemaining(
            now_ms,
            snapshot.timer_remaining,
            sizeof(snapshot.timer_remaining));
    }
    return snapshot;
}

static DiagnosticsSnapshot make_diagnostics_snapshot()
{
    DiagnosticsSnapshot snapshot = {
        digitalRead(GPIO_CLOCK) == LOW,
        digitalRead(GPIO_ALARM) == LOW,
        digitalRead(GPIO_FLOPPY) == LOW,
        input_service.discreteTouchPressed(),
        active_app && active_app->touchscreenAvailable(),
        digitalRead(GPIO_CHARGING) == HIGH,
        input_service.encoderPosition(),
        {},
        {},
        wifi_service.snapshot()};

    static constexpr uint8_t addresses[] = {
        0x18, 0x38, 0x40, 0x47, 0x50, 0x68};
    size_t length = 0;
    for (uint8_t address : addresses)
    {
        if (!i2c_bus.present(address))
            continue;
        const int written = snprintf(
            snapshot.i2c_devices + length,
            sizeof(snapshot.i2c_devices) - length,
            "%s0x%02X",
            length ? " " : "",
            address);
        if (written <= 0)
            break;
        length += static_cast<size_t>(written);
        if (length >= sizeof(snapshot.i2c_devices))
        {
            snapshot.i2c_devices[
                sizeof(snapshot.i2c_devices) - 1] = '\0';
            break;
        }
    }
    format_rtc_health(
        snapshot.rtc_status, sizeof(snapshot.rtc_status));
    return snapshot;
}

void MaclockApp::snoozeActiveAlarm()
{
    if (active_alarm_index_ < 0)
        return;

    audio_service.stop();
    alarm_service.snooze((size_t)active_alarm_index_, rtc_now());
    active_alarm_index_ = -1;
    requestState(UiState::Normal);
}

void MaclockApp::dismissActiveAlarm()
{
    if (active_alarm_index_ < 0)
        return;

    audio_service.stop();
    alarm_service.dismiss();
    active_alarm_index_ = -1;
    requestState(UiState::Normal);
}

void MaclockApp::dismissTimer()
{
    audio_service.stop();
    requestState(UiState::Normal);
}

static bool format_rtc_health(char *text, size_t text_size)
{
    return rtc_service.formatHealth(text, text_size);
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
    if (calibration_view.cross &&
        !lv_obj_has_flag(calibration_view.cross, LV_OBJ_FLAG_HIDDEN))
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
    if (!calibration_view.cross)
        return;
    lv_obj_t *scr = lv_screen_active();
    lv_obj_update_layout(calibration_view.cross);
    int w = lv_obj_get_width(calibration_view.cross);
    int h = lv_obj_get_height(calibration_view.cross);
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
    lv_obj_set_pos(calibration_view.cross, x, y);
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
        settings_store.saveBootBrightness(choice);

    int brightness = settings_store.loadBrightness();
    if (choice == BOOT_BRIGHTNESS_LOWEST)
        brightness = 1;
    else if (choice == BOOT_BRIGHTNESS_HIGHEST)
        brightness = kBrightnessMax;

    input_service.setEncoderPosition(brightness);
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
        !rtc_service.available() ||
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
        !rtc_service.available() ||
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

    if (chime_quiet_now(current) || audio_service.running())
        return;

    if (g_chime.volume >= kAudioVolumeLevelCount)
        return;
    audio_service.play(
        SoundSelector::resolvePath(
            g_chime_sound_path, "/quack.mp3"),
        audio_volume_from_index(g_chime.volume));
}

#include "ui/boot_options_view_events.cpp"
#include "ui/boot_options_view_layout.cpp"
#include "ui/ui_shell.cpp"
#include "ui/clock_view_common.cpp"
#include "ui/clock_view_custom.cpp"
#include "ui/clock_view_faces.cpp"
#include "ui/clock_view_screensavers.cpp"
#include "ui/clock_view_screensavers_extended.cpp"
#include "ui/ui_assets.cpp"
#include "ui/mqtt_notification_view.cpp"
#include "ui/maclock_state_machine.cpp"
#include "ui/control_panel_bindings.cpp"

#ifdef MACLOCK_LOCAL
bool MaclockApp::showLocalManualPage(uint8_t page)
{
    if (page >= BOOT_OPTIONS_PAGE_COUNT)
        return false;
    if (current_state_ == UiState::BootOptions)
    {
        boot_options_view.setPage(
            static_cast<BootOptionsPage>(page));
        return true;
    }
    boot_options_view.page_on_show =
        static_cast<BootOptionsPage>(page);
    requestState(UiState::BootOptions);
    return true;
}
#endif
