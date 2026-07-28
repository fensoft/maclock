#pragma once

#include <Arduino.h>

#include <memory>

namespace fs
{
enum SeekMode
{
    SeekSet,
    SeekCur,
    SeekEnd
};

class File
{
public:
    File();

    explicit operator bool() const;
    bool isDirectory() const;
    const char *path() const;
    const char *name() const;
    File openNextFile();
    void close();
    int read();
    size_t read(uint8_t *buffer, size_t size);
    size_t write(const uint8_t *buffer, size_t size);
    bool seek(uint32_t position, SeekMode mode = SeekSet);
    size_t position() const;
    size_t size() const;

    struct State;
    explicit File(std::shared_ptr<State> state);

private:
    std::shared_ptr<State> state_;
};
} // namespace fs

using File = fs::File;
