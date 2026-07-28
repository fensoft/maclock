#pragma once

#include <FS.h>

class AudioFileSourceLittleFS
{
public:
    explicit AudioFileSourceLittleFS(const char *path);
    ~AudioFileSourceLittleFS();

    size_t read(void *buffer, size_t bytes);
    bool seek(int64_t offset, int origin);
    int64_t tell() const;
    int64_t size() const;
    bool valid() const;

private:
    fs::File file_;
};
