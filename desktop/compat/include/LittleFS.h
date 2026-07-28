#pragma once

#include <FS.h>

class LittleFSFS
{
public:
    bool begin(bool format_on_fail = false);
    bool exists(const char *path) const;
    fs::File open(const char *path, const char *mode = "r");
    bool remove(const char *path);
    bool rename(const char *from, const char *to);
};

extern LittleFSFS LittleFS;
