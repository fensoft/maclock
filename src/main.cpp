#include <lvgl.h>
#include "AudioFileSourceLittleFS.h"
#include "AudioGeneratorOpus.h"
#include "AudioOutputI2S.h"
#include "AudioGeneratorMP3.h"
#include "es8311.h"
#include <esp_heap_caps.h>

AudioFileSourceLittleFS *file;
extern AudioOutputI2S *audio_out;
extern es8311_handle_t es8311_handle;
AudioGeneratorMP3 *mp3;

void setup_codec();
void setup_lvgl_display();
void setup_lvgl_input();
void lvgl_fs_init_littlefs();

void setup()
{
    Serial.begin(115200);
    delay(2000);
    heap_caps_malloc_extmem_enable(0);
    setup_codec();
    setup_lvgl_display();
    setup_lvgl_input();
    lvgl_fs_init_littlefs();

    file = new AudioFileSourceLittleFS("/startup.mp3");
    mp3 = new AudioGeneratorMP3();
    mp3->begin(file, audio_out);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_t *img = lv_image_create(scr);
    lv_image_set_src(img, "S:/boot.png");
    lv_obj_center(img);
}

void loop()
{
    if (mp3->isRunning())
    {
        if (!mp3->loop())
        {
            mp3->stop();
            Serial.println("MP3 done");
        }
    }

    uint32_t now = millis();
    static uint32_t last_lv = 0;
    if (now - last_lv >= 5) {
        last_lv = now;
        lv_timer_handler();
    }
}