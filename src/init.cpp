#include <lvgl.h>
#include <TFT_eSPI.h>
#include "es8311.h"
#include <LittleFS.h>
#include "touch.h"
#include "AudioOutputI2S.h"

TFT_eSPI my_lcd = TFT_eSPI();
static lv_display_t *disp;
static uint8_t buf1[LCD_W * LCD_H * 2];
static lv_indev_t *indev;
es8311_handle_t es8311_handle = nullptr;
AudioOutputI2S *audio_out;

static void my_lvgl_log_cb(lv_log_level_t level, const char *msg)
{
  Serial.print("[LVGL] ");
  Serial.print(msg);
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    static bool cleared = false;
    if(!cleared) {
        my_lcd.setAddrWindow(0, 0, LCD_W, LCD_H);
        my_lcd.fillScreen(0x0000);
        cleared = true;
    }

    my_lcd.setAddrWindow(area->x1, area->y1 + 16, w, h);
    my_lcd.pushColors((uint16_t *)px_map, w * h, true);
    lv_display_flush_ready(disp);
}

void setup_lvgl_display()
{
    my_lcd.init();
    my_lcd.fillScreen(0xFFFF);
    my_lcd.setRotation(3);

    lv_init();
    lv_log_register_print_cb(my_lvgl_log_cb);
    lv_tick_set_cb(millis);
    disp = lv_display_create(my_lcd.width() - 16, my_lcd.height() - 16);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_antialiasing(disp, false);

    lv_display_set_buffers(
        disp,
        buf1,
        NULL,
        sizeof(buf1),
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
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

void setup_lvgl_input()
{
    touch_init(my_lcd.width(), my_lcd.height(), my_lcd.getRotation());
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
}

void lvgl_fs_init_littlefs()
{
    LittleFS.begin();
    static lv_fs_drv_t drv;
    lv_fs_drv_init(&drv);

    drv.letter = 'S';

    drv.open_cb = [](lv_fs_drv_t *, const char *path, lv_fs_mode_t mode) -> void *
    {
        char full_path[256];
        if (path[0] == '/')
            snprintf(full_path, sizeof(full_path), "%s", path);
        else
            snprintf(full_path, sizeof(full_path), "/%s", path);

        const char *m = (mode == LV_FS_MODE_WR) ? "w" : "r";
        fs::File f = LittleFS.open(full_path, m);
        if (!f)
            return nullptr;

        return new fs::File(f);
    };

    drv.close_cb = [](lv_fs_drv_t *, void *file_p) -> lv_fs_res_t
    {
        fs::File *fp = (fs::File *)file_p;
        fp->close();
        delete fp;
        return LV_FS_RES_OK;
    };

    drv.read_cb = [](lv_fs_drv_t *, void *file_p,
                     void *buf, uint32_t btr, uint32_t *br) -> lv_fs_res_t
    {
        fs::File *fp = (fs::File *)file_p;
        *br = fp->read((uint8_t *)buf, btr);
        return LV_FS_RES_OK;
    };

    drv.seek_cb = [](lv_fs_drv_t *, void *file_p,
                     uint32_t pos, lv_fs_whence_t whence) -> lv_fs_res_t
    {
        fs::File *fp = (fs::File *)file_p;
        fs::SeekMode sm = fs::SeekSet;
        if (whence == LV_FS_SEEK_CUR)
            sm = fs::SeekCur;
        else if (whence == LV_FS_SEEK_END)
            sm = fs::SeekEnd;
        fp->seek(pos, sm);
        return LV_FS_RES_OK;
    };

    drv.tell_cb = [](lv_fs_drv_t *, void *file_p, uint32_t *pos) -> lv_fs_res_t
    {
        fs::File *fp = (fs::File *)file_p;
        *pos = fp->position();
        return LV_FS_RES_OK;
    };

    lv_fs_drv_register(&drv);
}

void setup_codec() {
    pinMode(I2S_EN, OUTPUT);
    digitalWrite(I2S_EN, LOW);
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);
    es8311_handle = es8311_create(&Wire, ES8311_ADDRESS_0);
    const es8311_clock_config_t es_clk = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = 44100 * 256,
        .sample_frequency = 44100,
    };
    es8311_init(es8311_handle, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
    es8311_voice_volume_set(es8311_handle, 80, NULL);
    es8311_voice_mute(es8311_handle, false);

    audio_out = new AudioOutputI2S();
    audio_out->SetPinout(I2S_BCK, I2S_WS, I2S_DOUT, I2S_MCK);
    audio_out->SetBuffers(8, 8192);
}
