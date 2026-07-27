#pragma once

#include <RTClib.h>

#include "i2c_bus.h"

class RtcService
{
public:
    enum class Type : uint8_t
    {
        None,
        Ds1307,
        Ds3231
    };

    explicit RtcService(I2cBus &bus) : bus_(bus) {}

    bool begin();
    DateTime now();
    void adjust(const DateTime &date_time);
    bool formatHealth(char *text, size_t text_size);

    Type type() const;
    bool available() const;
    const char *name() const;

private:
    bool probeDs1307();

    I2cBus &bus_;
    RTC_DS1307 ds1307_;
    RTC_DS3231 ds3231_;
    Type type_ = Type::None;
};
