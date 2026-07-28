#pragma once

#include <atomic>
#include <cstdint>

enum class puType
{
    none,
    up,
    down
};

class ESP32Encoder
{
public:
    inline static puType useInternalWeakPullResistors = puType::none;

    void attachHalfQuad(int pin_a, int pin_b);
    int64_t getCount() const;
    void setCount(int64_t count);
    void localAdd(int64_t delta);

private:
    std::atomic<int64_t> count_{0};
};
