#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include <driver/i2s_std.h>

class AudioOutputI2S
{
public:
    AudioOutputI2S();
    virtual ~AudioOutputI2S();

    virtual bool begin();
    virtual bool stop();
    virtual bool ConsumeSample(int16_t sample[2]);
    virtual bool flush();

    void SetPinout(int bclk, int word_select, int data, int mclk);
    void SetBuffers(int count, int bytes);
    void SetRate(int sample_rate);
    void SetChannels(int channels);
    void SetGain(float gain);

protected:
    void flushPending();

public:
    bool i2sOn = false;
    void *_tx_handle = nullptr;

private:
    int sample_rate_ = 44100;
    int channels_ = 2;
    float gain_ = 1.0f;
    std::vector<int16_t> pending_;
};
