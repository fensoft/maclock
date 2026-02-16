#include <LittleFS.h>

class M5UnifiedLittleFS {
public:
    template <typename... Args>
    bool begin(Args&&... args) {
        ((void)args, ...);
        return LittleFS.begin();
    }

    void end() { LittleFS.end(); }

    template <typename... Args>
    File open(Args&&... args) {
        return LittleFS.open(args...);
    }

    bool exists(const char* path) {
        return LittleFS.exists(path);
    }

    bool remove(const char* path) {
        return LittleFS.remove(path);
    }

    uint64_t cardSize() const {
        return static_cast<uint64_t>(LittleFS.totalBytes());
    }
};

inline M5UnifiedLittleFS SD;