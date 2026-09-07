#include <AudioFileSourceLittleFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <LittleFS.h>

#include "maclock_hal.h"

#include <miniaudio.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

AudioFileSourceLittleFS::AudioFileSourceLittleFS(
    const char *path)
    : file_(LittleFS.open(path, "r"))
{
}

AudioFileSourceLittleFS::~AudioFileSourceLittleFS()
{
    file_.close();
}

size_t AudioFileSourceLittleFS::read(
    void *buffer, size_t bytes)
{
    return file_.read(static_cast<uint8_t *>(buffer), bytes);
}

bool AudioFileSourceLittleFS::seek(
    int64_t offset, int origin)
{
    fs::SeekMode mode = fs::SeekSet;
    if (origin == SEEK_CUR)
        mode = fs::SeekCur;
    else if (origin == SEEK_END)
        mode = fs::SeekEnd;
    return offset >= 0 &&
           file_.seek(static_cast<uint32_t>(offset), mode);
}

int64_t AudioFileSourceLittleFS::tell() const
{
    return static_cast<int64_t>(file_.position());
}

int64_t AudioFileSourceLittleFS::size() const
{
    return static_cast<int64_t>(file_.size());
}

bool AudioFileSourceLittleFS::valid() const
{
    return static_cast<bool>(file_);
}

AudioOutputI2S::AudioOutputI2S()
{
    pending_.reserve(1024);
}

AudioOutputI2S::~AudioOutputI2S()
{
    stop();
}

bool AudioOutputI2S::begin()
{
    if (i2sOn)
        return true;
    i2sOn = maclock_hal().audio().begin(
        static_cast<uint32_t>(sample_rate_),
        static_cast<uint8_t>(channels_));
    _tx_handle = this;
    return i2sOn;
}

bool AudioOutputI2S::stop()
{
    if (!i2sOn)
        return true;
    flushPending();
    maclock_hal().audio().stop();
    i2sOn = false;
    _tx_handle = nullptr;
    return true;
}

bool AudioOutputI2S::ConsumeSample(int16_t sample[2])
{
    if (!sample || !i2sOn)
        return false;
    const float limited_gain = std::clamp(gain_, 0.0f, 1.0f);
    pending_.push_back(static_cast<int16_t>(
        std::clamp(
            sample[0] * limited_gain,
            -32768.0f, 32767.0f)));
    pending_.push_back(static_cast<int16_t>(
        std::clamp(
            sample[1] * limited_gain,
            -32768.0f, 32767.0f)));
    if (pending_.size() >= 1024)
        flushPending();
    return true;
}

bool AudioOutputI2S::flush()
{
    flushPending();
    maclock_hal().audio().drain();
    return true;
}

void AudioOutputI2S::SetPinout(int, int, int, int)
{
}

void AudioOutputI2S::SetBuffers(int, int)
{
}

void AudioOutputI2S::SetRate(int sample_rate)
{
    const int new_rate = std::max(8000, sample_rate);
    if (sample_rate_ == new_rate)
        return;
    sample_rate_ = new_rate;
    if (i2sOn)
    {
        maclock_hal().audio().begin(
            static_cast<uint32_t>(sample_rate_),
            static_cast<uint8_t>(channels_));
    }
}

void AudioOutputI2S::SetChannels(int channels)
{
    const int new_channels = std::clamp(channels, 1, 2);
    if (channels_ == new_channels)
        return;
    channels_ = new_channels;
    if (i2sOn)
    {
        maclock_hal().audio().begin(
            static_cast<uint32_t>(sample_rate_),
            static_cast<uint8_t>(channels_));
    }
}

void AudioOutputI2S::SetGain(float gain)
{
    gain_ = std::clamp(gain, 0.0f, 1.0f);
}

void AudioOutputI2S::flushPending()
{
    if (pending_.empty())
        return;
    maclock_hal().audio().write(
        pending_.data(), pending_.size() / 2, 2);
    pending_.clear();
}

struct AudioGeneratorMP3::State
{
    AudioFileSourceLittleFS *source = nullptr;
    AudioOutputI2S *output = nullptr;
    ma_decoder decoder{};
    bool initialized = false;
    bool running = false;

    static ma_result read(
        ma_decoder *decoder,
        void *buffer,
        size_t bytes_to_read,
        size_t *bytes_read)
    {
        auto *self = static_cast<State *>(decoder->pUserData);
        *bytes_read = self->source->read(
            buffer, bytes_to_read);
        return MA_SUCCESS;
    }

    static ma_result seek(
        ma_decoder *decoder,
        ma_int64 offset,
        ma_seek_origin origin)
    {
        auto *self = static_cast<State *>(decoder->pUserData);
        int native_origin = SEEK_SET;
        if (origin == ma_seek_origin_current)
            native_origin = SEEK_CUR;
        return self->source->seek(offset, native_origin)
                   ? MA_SUCCESS
                   : MA_ERROR;
    }
};

AudioGeneratorMP3::AudioGeneratorMP3()
    : state_(new State())
{
}

AudioGeneratorMP3::~AudioGeneratorMP3()
{
    stop();
}

bool AudioGeneratorMP3::begin(
    AudioFileSourceLittleFS *source,
    AudioOutputI2S *output)
{
    if (!source || !source->valid() || !output)
        return false;
    stop();
    state_->source = source;
    state_->output = output;
    ma_decoder_config config =
        ma_decoder_config_init(ma_format_s16, 2, 0);
    if (ma_decoder_init(
            State::read, State::seek,
            state_.get(), &config,
            &state_->decoder) != MA_SUCCESS)
    {
        return false;
    }
    ma_format format = ma_format_unknown;
    ma_uint32 channels = 0;
    ma_uint32 sample_rate = 0;
    if (ma_decoder_get_data_format(
            &state_->decoder, &format, &channels,
            &sample_rate, nullptr, 0) != MA_SUCCESS ||
        format != ma_format_s16 || channels != 2 || !sample_rate)
    {
        ma_decoder_uninit(&state_->decoder);
        return false;
    }
    output->SetRate(static_cast<int>(sample_rate));
    output->SetChannels(2);
    state_->initialized = true;
    state_->running = output->begin();
    return state_->running;
}

bool AudioGeneratorMP3::loop()
{
    if (!state_->running)
        return false;
    int16_t frames[512 * 2];
    ma_uint64 decoded = 0;
    const ma_result result = ma_decoder_read_pcm_frames(
        &state_->decoder, frames, 512, &decoded);
    for (ma_uint64 frame = 0; frame < decoded; ++frame)
    {
        int16_t sample[2] = {
            frames[frame * 2],
            frames[frame * 2 + 1]};
        if (!state_->output->ConsumeSample(sample))
            return true;
    }
    if (decoded == 0 || result == MA_AT_END)
    {
        state_->output->flush();
        state_->running = false;
        return false;
    }
    return true;
}

bool AudioGeneratorMP3::stop()
{
    state_->running = false;
    if (state_->initialized)
    {
        ma_decoder_uninit(&state_->decoder);
        state_->initialized = false;
    }
    return true;
}

bool AudioGeneratorMP3::isRunning() const
{
    return state_->running;
}
