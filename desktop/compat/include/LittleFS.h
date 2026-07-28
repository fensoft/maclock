#pragma once

#include <FS.h>

class LittleFSFS
{
public:
    bool begin(bool format_on_fail = false);
    bool exists(const char *path) const;
    fs::File open(const char *path, const char *mode = "r");
};

extern LittleFSFS LittleFS;
