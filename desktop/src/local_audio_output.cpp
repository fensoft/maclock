#include "local_audio_output.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
constexpr size_t kAudioBufferFrames = 44100;
constexpr size_t kAudioBufferSamples = kAudioBufferFrames * 2;
}

struct LocalAudioOutput::Impl
{
    Impl() : samples(kAudioBufferSamples) {}

    mutable std::mutex configuration_mutex;
    std::mutex wait_mutex;
    std::condition_variable drained;
    std::vector<int16_t> samples;
    std::atomic<uint64_t> read_position{0};
    std::atomic<uint64_t> write_position{0};
    ma_device device{};
    bool initialized = false;
    std::atomic<bool> available{true};
    std::atomic<uint32_t> rate{44100};
    uint8_t channels = 2;
    std::atomic<uint8_t> volume{0};
    std::atomic<bool> muted{true};
    std::atomic<bool> can_drain{false};
    std::atomic<bool> primed{false};
    std::atomic<bool> playback_started{false};

    static void callback(
        ma_device *device, void *output,
        const void *, ma_uint32 frame_count)
    {
        auto *self =
            static_cast<Impl *>(device->pUserData);
        auto *destination =
            static_cast<int16_t *>(output);
        const size_t requested_samples =
            static_cast<size_t>(frame_count) * 2;
        std::memset(
            destination, 0,
            requested_samples * sizeof(int16_t));
        if (!self->can_drain.load(std::memory_order_relaxed) ||
            !self->primed.load(std::memory_order_acquire))
        {
            return;
        }

        const uint64_t read = self->read_position.load(
            std::memory_order_relaxed);
        const uint64_t written = self->write_position.load(
            std::memory_order_acquire);
        const size_t available_samples = static_cast<size_t>(
            std::min<uint64_t>(written - read, requested_samples));
        const uint8_t volume = self->volume.load(
            std::memory_order_relaxed);
        const bool muted = self->muted.load(
            std::memory_order_relaxed);
        for (size_t index = 0; index < available_samples; ++index)
        {
            if (!muted)
            {
                const int32_t scaled =
                    static_cast<int32_t>(
                        self->samples[(read + index) %
                                      kAudioBufferSamples]) *
                    volume / 100;
                destination[index] = static_cast<int16_t>(scaled);
            }
        }
        self->read_position.store(
            read + available_samples, std::memory_order_release);
        if (available_samples < requested_samples)
            self->primed.store(false, std::memory_order_release);
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
    if (impl_->initialized && impl_->rate.load() == sample_rate &&
        impl_->channels == channels)
        return true;
    impl_->can_drain = false;
    impl_->drained.notify_all();
    if (impl_->initialized)
    {
        ma_device_uninit(&impl_->device);
        impl_->initialized = false;
    }
    impl_->read_position = 0;
    impl_->write_position = 0;
    impl_->primed = false;
    impl_->playback_started = false;
    impl_->rate = sample_rate;
    impl_->channels = channels;
    ma_device_config config =
        ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = 2;
    config.sampleRate = sample_rate;
    config.periodSizeInMilliseconds = 20;
    config.periods = 3;
    config.performanceProfile = ma_performance_profile_conservative;
    config.dataCallback = Impl::callback;
    config.pUserData = impl_.get();
    if (ma_device_init(nullptr, &config, &impl_->device) !=
        MA_SUCCESS)
    {
        impl_->available = false;
        impl_->can_drain = false;
        std::cerr
            << "Local audio output unavailable; continuing muted\n";
        return true;
    }
    impl_->can_drain = true;
    if (ma_device_start(&impl_->device) != MA_SUCCESS)
    {
        ma_device_uninit(&impl_->device);
        impl_->available = false;
        impl_->can_drain = false;
        std::cerr
            << "Local audio output could not start; continuing muted\n";
        return true;
    }
    impl_->initialized = true;
    impl_->available = true;
    return true;
}

void LocalAudioOutput::stop()
{
    std::lock_guard<std::mutex> configuration_lock(
        impl_->configuration_mutex);
    impl_->can_drain = false;
    impl_->drained.notify_all();
    if (impl_->initialized)
    {
        ma_device_uninit(&impl_->device);
        impl_->initialized = false;
    }
    impl_->read_position = 0;
    impl_->write_position = 0;
    impl_->primed = false;
    impl_->playback_started = false;
    impl_->drained.notify_all();
}

size_t LocalAudioOutput::write(
    const int16_t *samples, size_t frame_count,
    uint8_t channels)
{
    if (!samples || !frame_count)
        return 0;
    if (!impl_->can_drain.load())
    {
        const bool silent_output = !impl_->available.load();
        const uint32_t rate = impl_->rate.load();
        if (silent_output && rate)
        {
            std::this_thread::sleep_for(
                std::chrono::microseconds(
                    frame_count * 1000000ULL / rate));
        }
        return frame_count;
    }

    size_t written_frames = 0;
    while (written_frames < frame_count)
    {
        if (!impl_->can_drain.load())
            return written_frames;

        const uint64_t read = impl_->read_position.load(
            std::memory_order_acquire);
        const uint64_t written = impl_->write_position.load(
            std::memory_order_relaxed);
        if (written - read >= kAudioBufferSamples)
        {
            std::unique_lock<std::mutex> lock(impl_->wait_mutex);
            impl_->drained.wait(
                lock,
                [this]()
                {
                    const uint64_t current_read =
                        impl_->read_position.load(
                            std::memory_order_acquire);
                    const uint64_t current_write =
                        impl_->write_position.load(
                            std::memory_order_relaxed);
                    return !impl_->can_drain.load() ||
                           current_write - current_read <
                               kAudioBufferSamples;
                });
            continue;
        }
        const size_t available_frames =
            (kAudioBufferSamples -
             static_cast<size_t>(written - read)) /
            2;
        const size_t copied_frames = std::min(
            frame_count - written_frames, available_frames);
        for (size_t frame = 0; frame < copied_frames; ++frame)
        {
            const size_t source_frame = written_frames + frame;
            const int16_t left = samples[source_frame * channels];
            const int16_t right = channels == 1
                                      ? left
                                      : samples[source_frame * channels + 1];
            const uint64_t destination = written + frame * 2;
            impl_->samples[destination % kAudioBufferSamples] = left;
            impl_->samples[(destination + 1) %
                           kAudioBufferSamples] = right;
        }
        impl_->write_position.store(
            written + copied_frames * 2,
            std::memory_order_release);
        const uint64_t queued_samples =
            written + copied_frames * 2 -
            impl_->read_position.load(std::memory_order_acquire);
        const uint32_t rate = impl_->rate.load();
        const bool playback_started = impl_->playback_started.load(
            std::memory_order_relaxed);
        const uint64_t prebuffer_samples =
            std::max<uint32_t>(
                rate >= 40000
                    ? (playback_started ? rate / 16 : rate / 4)
                    : rate / 25,
                1) *
            2ULL;
        if (queued_samples >= prebuffer_samples)
        {
            impl_->primed.store(true, std::memory_order_release);
            impl_->playback_started.store(
                true, std::memory_order_release);
        }
        written_frames += copied_frames;
    }
    return frame_count;
}

void LocalAudioOutput::drain()
{
    impl_->primed.store(true, std::memory_order_release);
    std::unique_lock<std::mutex> lock(impl_->wait_mutex);
    impl_->drained.wait(
        lock,
        [this]()
        {
            return impl_->read_position.load(
                       std::memory_order_acquire) >=
                       impl_->write_position.load(
                           std::memory_order_acquire) ||
                   !impl_->can_drain.load();
        });
}

void LocalAudioOutput::setVolume(uint8_t volume)
{
    impl_->volume = std::min<uint8_t>(volume, 100);
}

void LocalAudioOutput::setMuted(bool muted)
{
    impl_->muted = muted;
    if (muted)
    {
        impl_->read_position.store(
            impl_->write_position.load(std::memory_order_acquire),
            std::memory_order_release);
        impl_->primed.store(false, std::memory_order_release);
        impl_->playback_started.store(
            false, std::memory_order_release);
    }
    if (muted)
        impl_->drained.notify_all();
}

LocalAudioSnapshot LocalAudioOutput::snapshot() const
{
    std::lock_guard<std::mutex> lock(impl_->configuration_mutex);
    return {
        impl_->available.load(),
        impl_->muted.load(),
        impl_->rate.load(),
        impl_->volume.load()};
}
