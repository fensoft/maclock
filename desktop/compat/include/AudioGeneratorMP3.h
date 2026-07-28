#pragma once

#include <memory>

class AudioFileSourceLittleFS;
class AudioOutputI2S;

class AudioGeneratorMP3
{
public:
    AudioGeneratorMP3();
    ~AudioGeneratorMP3();

    bool begin(
        AudioFileSourceLittleFS *source,
        AudioOutputI2S *output);
    bool loop();
    bool stop();
    bool isRunning() const;

private:
    struct State;
    std::unique_ptr<State> state_;
};
