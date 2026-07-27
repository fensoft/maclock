#pragma once

#include <AudioFileSourceLittleFS.h>
#include <AudioGeneratorMP3.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

class DisplayService;

class AudioService
{
public:
    explicit AudioService(DisplayService &display);

    void begin();
    bool startTask();
    void suspendTask();
    void resumeTask();

    bool play(const char *path, uint8_t volume);
    void stop();
    bool takeFinished();
    bool running();

private:
    static void taskEntry(void *context);
    void runTask();
    void deletePlaybackLocked();

    DisplayService &display_;
    AudioFileSourceLittleFS *file_ = nullptr;
    AudioGeneratorMP3 *decoder_ = nullptr;
    bool finished_ = false;
    portMUX_TYPE finished_mux_ = portMUX_INITIALIZER_UNLOCKED;
    SemaphoreHandle_t lock_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
};

void bind_audio_preview(AudioService &audio);
bool audio_preview_play(const char *path, uint8_t volume);
