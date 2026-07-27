#include "audio_service.h"

#include "display_service.h"
#include "es8311.h"

namespace
{
AudioService *g_preview_audio = nullptr;
}

AudioService::AudioService(DisplayService &display)
    : display_(display)
{
}

void AudioService::begin()
{
    if (!lock_)
        lock_ = xSemaphoreCreateMutex();
    bind_audio_preview(*this);
}

bool AudioService::startTask()
{
    if (task_handle_)
        return true;
    return xTaskCreatePinnedToCore(
               taskEntry,
               "audio_task",
               4096,
               this,
               2,
               &task_handle_,
               0) == pdPASS;
}

void AudioService::suspendTask()
{
    if (task_handle_)
        vTaskSuspend(task_handle_);
}

void AudioService::resumeTask()
{
    if (task_handle_)
        vTaskResume(task_handle_);
}

void AudioService::deletePlaybackLocked()
{
    if (decoder_)
    {
        if (decoder_->isRunning())
            decoder_->stop();
        delete decoder_;
        decoder_ = nullptr;
    }
    if (file_)
    {
        delete file_;
        file_ = nullptr;
    }
}

void AudioService::stop()
{
    if (lock_)
        xSemaphoreTake(lock_, portMAX_DELAY);
    deletePlaybackLocked();
    portENTER_CRITICAL(&finished_mux_);
    finished_ = false;
    portEXIT_CRITICAL(&finished_mux_);
    if (lock_)
        xSemaphoreGive(lock_);
}

bool AudioService::play(const char *path, uint8_t volume)
{
    AudioOutputI2S *audio_output = display_.audioOutput();
    if (!path || !audio_output)
        return false;

    if (lock_)
        xSemaphoreTake(lock_, portMAX_DELAY);
    deletePlaybackLocked();
    const es8311_handle_t codec = display_.codec();
    if (codec)
        es8311_voice_volume_set(
            codec, volume, nullptr);
    audio_output->SetGain(1.0f);

    file_ = new AudioFileSourceLittleFS(path);
    decoder_ = new AudioGeneratorMP3();
    const bool started =
        file_ && decoder_ &&
        decoder_->begin(file_, audio_output);
    if (!started)
        deletePlaybackLocked();

    portENTER_CRITICAL(&finished_mux_);
    finished_ = false;
    portEXIT_CRITICAL(&finished_mux_);
    if (lock_)
        xSemaphoreGive(lock_);
    return started;
}

bool AudioService::takeFinished()
{
    portENTER_CRITICAL(&finished_mux_);
    const bool finished = finished_;
    finished_ = false;
    portEXIT_CRITICAL(&finished_mux_);
    return finished;
}

bool AudioService::running()
{
    if (lock_)
        xSemaphoreTake(lock_, portMAX_DELAY);
    const bool is_running = decoder_ && decoder_->isRunning();
    if (lock_)
        xSemaphoreGive(lock_);
    return is_running;
}

void AudioService::taskEntry(void *context)
{
    static_cast<AudioService *>(context)->runTask();
}

void AudioService::runTask()
{
    for (;;)
    {
        bool is_running = false;
        if (lock_)
            xSemaphoreTake(lock_, portMAX_DELAY);
        if (decoder_ && decoder_->isRunning())
        {
            is_running = true;
            if (!decoder_->loop())
            {
                decoder_->stop();
                is_running = false;
                portENTER_CRITICAL(&finished_mux_);
                finished_ = true;
                portEXIT_CRITICAL(&finished_mux_);
            }
        }
        if (lock_)
            xSemaphoreGive(lock_);
        vTaskDelay(pdMS_TO_TICKS(is_running ? 1 : 10));
    }
}

void bind_audio_preview(AudioService &audio)
{
    g_preview_audio = &audio;
}

bool audio_preview_play(const char *path, uint8_t volume)
{
    return g_preview_audio &&
           g_preview_audio->play(path, volume);
}
