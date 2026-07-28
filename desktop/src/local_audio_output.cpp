#include "local_audio_output.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>

namespace
{
constexpr size_t kMaximumAudioFrames = 44100 / 2;
}

struct LocalAudioOutput::Impl
{
    mutable std::mutex configuration_mutex;
    mutable std::mutex mutex;
    std::condition_variable drained;
    std::deque<int16_t> samples;
    ma_device device{};
    bool initialized = false;
    bool available = true;
    uint32_t rate = 44100;
    uint8_t channels = 2;
    uint8_t volume = 0;
    bool muted = true;
    bool can_drain = false;

    static void callback(
        ma_device *device, void *output,
        const void *, ma_uint32 frame_count)
    {
        auto *self =
            static_cast<Impl *>(device->pUserData);
        auto *destination =
            static_cast<int16_t *>(output);
        {
            std::lock_guard<std::mutex> lock(self->mutex);
            const float gain = self->muted
                                   ? 0.0f
                                   : self->volume / 100.0f;
            for (ma_uint32 frame = 0;
                 frame < frame_count; ++frame)
            {
                for (int channel = 0; channel < 2; ++channel)
                {
                    int16_t value = 0;
                    if (!self->samples.empty())
                    {
                        value = self->samples.front();
                        self->samples.pop_front();
                    }
                    const float limited = std::clamp(
                        value * gain,
                        -32768.0f, 32767.0f);
                    destination[frame * 2 + channel] =
                        static_cast<int16_t>(limited);
                }
            }
        }
        self->drained.notify_all();
    }
};

LocalAudioOutput::LocalAudioOutput()
    : impl_(new Impl())
{
}

LocalAudioOutput::~LocalAudioOutput()
{
    stop();
}

bool LocalAudioOutput::start(
    uint32_t sample_rate, uint8_t channels)
{
    std::lock_guard<std::mutex> configuration_lock(
        impl_->configuration_mutex);
    if (impl_->initialized && impl_->rate == sample_rate)
        return true;
    if (impl_->initialized)
    {
        ma_device_uninit(&impl_->device);
        impl_->initialized = false;
    }
    impl_->rate = sample_rate;
    impl_->channels = channels;
    ma_device_config config =
        ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = 2;
    config.sampleRate = sample_rate;
    config.dataCallback = Impl::callback;
    config.pUserData = impl_.get();
    if (ma_device_init(nullptr, &config, &impl_->device) !=
        MA_SUCCESS)
    {
        impl_->available = false;
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->can_drain = false;
        std::cerr
            << "Local audio output unavailable; continuing muted\n";
        return true;
    }
    if (ma_device_start(&impl_->device) != MA_SUCCESS)
    {
        ma_device_uninit(&impl_->device);
        impl_->available = false;
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->can_drain = false;
        std::cerr
            << "Local audio output could not start; continuing muted\n";
        return true;
    }
    impl_->initialized = true;
    impl_->available = true;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->can_drain = true;
    }
    return true;
}

void LocalAudioOutput::stop()
{
    std::lock_guard<std::mutex> configuration_lock(
        impl_->configuration_mutex);
    if (impl_->initialized)
    {
        ma_device_uninit(&impl_->device);
        impl_->initialized = false;
    }
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->can_drain = false;
        impl_->samples.clear();
    }
    impl_->drained.notify_all();
}

size_t LocalAudioOutput::write(
    const int16_t *samples, size_t frame_count,
    uint8_t channels)
{
    if (!samples || !frame_count)
        return 0;
    std::unique_lock<std::mutex> lock(impl_->mutex);
    impl_->drained.wait(
        lock,
        [this, frame_count]()
        {
            return !impl_->can_drain ||
                   (frame_count > kMaximumAudioFrames &&
                    impl_->samples.empty()) ||
                   impl_->samples.size() / 2 +
                           frame_count <=
                       kMaximumAudioFrames;
        });
    if (!impl_->can_drain)
        return frame_count;
    const size_t new_samples = frame_count * 2;
    impl_->samples.resize(
        impl_->samples.size() + new_samples);
    const size_t first_new_sample =
        impl_->samples.size() - new_samples;
    for (size_t frame = 0; frame < frame_count; ++frame)
    {
        if (channels == 1)
        {
            const int16_t sample = samples[frame];
            impl_->samples[first_new_sample + frame * 2] =
                sample;
            impl_->samples[
                first_new_sample + frame * 2 + 1] =
                sample;
        }
        else
        {
            impl_->samples[first_new_sample + frame * 2] =
                samples[frame * channels];
            impl_->samples[
                first_new_sample + frame * 2 + 1] =
                samples[frame * channels + 1];
        }
    }
    return frame_count;
}

void LocalAudioOutput::drain()
{
    std::unique_lock<std::mutex> lock(impl_->mutex);
    impl_->drained.wait(
        lock,
        [this]()
        {
            return impl_->samples.empty() ||
                   !impl_->can_drain;
        });
}

void LocalAudioOutput::setVolume(uint8_t volume)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->volume = std::min<uint8_t>(volume, 100);
}

void LocalAudioOutput::setMuted(bool muted)
{
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->muted = muted;
        if (muted)
            impl_->samples.clear();
    }
    if (muted)
        impl_->drained.notify_all();
}

LocalAudioSnapshot LocalAudioOutput::snapshot() const
{
    std::scoped_lock lock(
        impl_->configuration_mutex, impl_->mutex);
    return {
        impl_->available,
        impl_->muted,
        impl_->rate,
        impl_->volume};
}
