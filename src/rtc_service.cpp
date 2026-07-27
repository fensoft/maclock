#include "rtc_service.h"

#include <Arduino.h>

#include "localization.h"

namespace
{
constexpr uint8_t kRtcAddress = 0x68;
constexpr uint8_t kDs3231ControlRegister = 0x0E;
constexpr uint8_t kDs3231ConvertTemperature = 0x20;
}

bool RtcService::probeDs1307()
{
    uint8_t original = 0;
    if (!bus_.readRegister(
            kRtcAddress, kDs3231ControlRegister, original))
        return false;
    if (!bus_.writeRegister(
            kRtcAddress,
            kDs3231ControlRegister,
            original | kDs3231ConvertTemperature))
    {
        return false;
    }
    delay(250);

    uint8_t after = 0;
    if (!bus_.readRegister(
            kRtcAddress, kDs3231ControlRegister, after))
        return false;

    const bool is_ds1307 =
        (after & kDs3231ConvertTemperature) != 0;
    bus_.writeRegister(
        kRtcAddress, kDs3231ControlRegister,
        is_ds1307
            ? original
            : (original & ~kDs3231ConvertTemperature));
    return is_ds1307;
}

bool RtcService::begin()
{
    type_ = Type::None;
    if (!bus_.present(kRtcAddress))
    {
        Serial.println("No RTC detected at 0x68");
        return false;
    }

    if (probeDs1307() && ds1307_.begin(&bus_.wire()))
    {
        type_ = Type::Ds1307;
        Serial.println("DS1307 detected at 0x68");
        return true;
    }

    if (ds3231_.begin(&bus_.wire()))
    {
        type_ = Type::Ds3231;
        Serial.println("DS3231 detected at 0x68");
        return true;
    }

    Serial.println("RTC at 0x68 is unsupported");
    return false;
}

DateTime RtcService::now()
{
    switch (type_)
    {
    case Type::Ds1307:
        return ds1307_.now();
    case Type::Ds3231:
        return ds3231_.now();
    default:
        return DateTime(2000, 1, 1, 0, 0, 0);
    }
}

void RtcService::adjust(const DateTime &date_time)
{
    switch (type_)
    {
    case Type::Ds1307:
        ds1307_.adjust(date_time);
        break;
    case Type::Ds3231:
        ds3231_.adjust(date_time);
        break;
    default:
        break;
    }
}

bool RtcService::formatHealth(char *text, size_t text_size)
{
    if (!text || text_size == 0)
        return false;
    if (type_ == Type::None)
    {
        snprintf(text, text_size, "%s", tr("RTC: not detected"));
        return false;
    }
    if (type_ == Type::Ds1307 && !ds1307_.isrunning())
    {
        snprintf(text, text_size, "%s",
                 tr("RTC: DS1307 stopped - check battery"));
        return false;
    }
    if (type_ == Type::Ds3231 && ds3231_.lostPower())
    {
        snprintf(text, text_size, "%s",
                 tr("RTC: DS3231 lost power - check battery"));
        return false;
    }

    const DateTime current = now();
    if (!current.isValid())
    {
        snprintf(text, text_size, "%s", tr("RTC: invalid date"));
        return false;
    }
    if (current.year() < 2024)
    {
        snprintf(text, text_size, tr("RTC: date not set (%04d)"),
                 current.year());
        return false;
    }
    snprintf(text, text_size, tr("RTC: %s OK"), name());
    return true;
}

RtcService::Type RtcService::type() const
{
    return type_;
}

bool RtcService::available() const
{
    return type_ != Type::None;
}

const char *RtcService::name() const
{
    switch (type_)
    {
    case Type::Ds1307:
        return "DS1307";
    case Type::Ds3231:
        return "DS3231";
    default:
        return "none";
    }
}
