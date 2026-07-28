#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

struct LocalAudioSnapshot
{
    bool available = false;
    bool muted = true;
    uint32_t sample_rate = 44100;
    uint8_t volume = 0;
};

class LocalAudioOutput
{
public:
    LocalAudioOutput();
    ~LocalAudioOutput();

    bool start(uint32_t sample_rate, uint8_t channels = 2);
    void stop();
    size_t write(
        const int16_t *samples, size_t frame_count,
        uint8_t channels);
    void drain();
    void setVolume(uint8_t volume);
    void setMuted(bool muted);
    LocalAudioSnapshot snapshot() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
