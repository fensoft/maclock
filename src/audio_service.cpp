#include "audio_service.h"

#include "audio_volume.h"
#include "display_service.h"
#include "es8311.h"

#ifndef MACLOCK_LOCAL
#include <AudioFileSourcePROGMEM.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#endif

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
    const es8311_handle_t codec = display_.codec();
    if (codec)
        es8311_voice_mute(codec, true);

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
    if (preloaded_data_)
    {
        free(preloaded_data_);
        preloaded_data_ = nullptr;
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

bool AudioService::play(
    const char *path, uint8_t volume,
    bool preload_to_memory)
{
    AudioOutputI2S *audio_output = display_.audioOutput();
    if (!path || !audio_output)
        return false;

    if (lock_)
        xSemaphoreTake(lock_, portMAX_DELAY);
    deletePlaybackLocked();
    const es8311_handle_t codec = display_.codec();
    if (codec)
        es8311_voice_volume_set(codec, 0, nullptr);
    audio_output->SetGain(1.0f);
    display_.prepareAudioPlayback();

#ifndef MACLOCK_LOCAL
    if (preload_to_memory)
    {
        fs::File source = LittleFS.open(path, "r");
        const size_t size = source ? source.size() : 0;
        if (size)
        {
            preloaded_data_ = static_cast<uint8_t *>(
                heap_caps_malloc(
                    size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (preloaded_data_ &&
                source.read(preloaded_data_, size) == size)
            {
                file_ = new AudioFileSourcePROGMEM(
                    preloaded_data_, size);
            }
            else if (preloaded_data_)
            {
                free(preloaded_data_);
                preloaded_data_ = nullptr;
            }
        }
        source.close();
    }
#else
    (void)preload_to_memory;
#endif
    if (!file_)
        file_ = new AudioFileSourceLittleFS(path);
    decoder_ = new AudioGeneratorMP3();
    const bool started =
        file_ && decoder_ &&
        decoder_->begin(file_, audio_output);
    if (!started)
        deletePlaybackLocked();
    else if (codec)
    {
        // AudioOutputI2S::begin() preloads DMA with silence. Keep the
        // ES8311 muted while its clocks settle, then reveal that silent
        // stream before the decoder task starts sending MP3 samples.
        vTaskDelay(pdMS_TO_TICKS(5));
        es8311_voice_mute(codec, false);
        es8311_voice_volume_set(
            codec, audio_volume_codec_level(volume), nullptr);
    }

    portENTER_CRITICAL(&finished_mux_);
    finished_ = false;
    portEXIT_CRITICAL(&finished_mux_);
    if (lock_)
        xSemaphoreGive(lock_);
    return started;
}

void AudioService::setVolume(uint8_t volume)
{
    if (lock_)
        xSemaphoreTake(lock_, portMAX_DELAY);
    const es8311_handle_t codec = display_.codec();
    if (codec && decoder_ && decoder_->isRunning())
    {
        es8311_voice_volume_set(
            codec, audio_volume_codec_level(volume), nullptr);
    }
    if (lock_)
        xSemaphoreGive(lock_);
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
                AudioOutputI2S *audio_output =
                    display_.audioOutput();
                if (audio_output)
                {
                    // The decoder has reached MP3 EOF, but DMA can still
                    // contain the last part of the sound. Queue silence
                    // until those samples have physically played before
                    // reporting completion or stopping I2S.
                    audio_output->flush();
                    // Allow the final DMA frame and the codec's analog
                    // output path to settle before muting. Without this
                    // short tail, the end of sounds such as the floppy
                    // loading sample can be clipped.
                    vTaskDelay(pdMS_TO_TICKS(75));
                }
                const es8311_handle_t codec = display_.codec();
                if (codec)
                    es8311_voice_mute(codec, true);
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
