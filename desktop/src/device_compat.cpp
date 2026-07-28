#include <Adafruit_BMP5xx.h>
#include <Adafruit_HTU21DF.h>
#include <RTClib.h>

#include "maclock_local_bridge.h"

#include <algorithm>
#include <ctime>

namespace
{
std::time_t make_epoch(
    int year, int month, int day,
    int hour, int minute, int second)
{
    std::tm value{};
    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = day;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_sec = second;
#if defined(__APPLE__) || defined(__linux__)
    return timegm(&value);
#else
    return std::mktime(&value);
#endif
}
} // namespace

DateTime::DateTime() = default;

DateTime::DateTime(uint32_t epoch)
{
    const std::time_t value = epoch;
    std::tm parts{};
    gmtime_r(&value, &parts);
    year_ = static_cast<uint16_t>(parts.tm_year + 1900);
    month_ = static_cast<uint8_t>(parts.tm_mon + 1);
    day_ = static_cast<uint8_t>(parts.tm_mday);
    hour_ = static_cast<uint8_t>(parts.tm_hour);
    minute_ = static_cast<uint8_t>(parts.tm_min);
    second_ = static_cast<uint8_t>(parts.tm_sec);
}

DateTime::DateTime(
    uint16_t year, uint8_t month, uint8_t day,
    uint8_t hour, uint8_t minute, uint8_t second)
    : year_(year),
      month_(month),
      day_(day),
      hour_(hour),
      minute_(minute),
      second_(second)
{
}

uint8_t DateTime::dayOfTheWeek() const
{
    const std::time_t epoch = unixtime();
    std::tm parts{};
    gmtime_r(&epoch, &parts);
    return static_cast<uint8_t>(parts.tm_wday);
}

uint32_t DateTime::unixtime() const
{
    return static_cast<uint32_t>(
        make_epoch(
            year_, month_, day_,
            hour_, minute_, second_));
}

bool DateTime::isValid() const
{
    if (year_ < 1970 || year_ > 2200 ||
        month_ < 1 || month_ > 12 ||
        day_ < 1 || day_ > 31 ||
        hour_ > 23 || minute_ > 59 || second_ > 59)
    {
        return false;
    }
    const std::time_t epoch = unixtime();
    std::tm parts{};
    gmtime_r(&epoch, &parts);
    return parts.tm_year + 1900 == year_ &&
           parts.tm_mon + 1 == month_ &&
           parts.tm_mday == day_;
}

bool RTC_DS1307::begin(TwoWire *wire)
{
    if (wire)
    {
        wire->beginTransmission(0x68);
        if (wire->endTransmission() != 0)
            return false;
    }
    return maclock_local_rtc_present() &&
           maclock_local_rtc_is_ds1307();
}

DateTime RTC_DS1307::now() const
{
    return DateTime(maclock_local_rtc_epoch());
}

void RTC_DS1307::adjust(const DateTime &date_time)
{
    maclock_local_rtc_adjust(date_time.unixtime());
}

bool RTC_DS1307::isrunning() const
{
    return maclock_local_rtc_present();
}

bool RTC_DS3231::begin(TwoWire *wire)
{
    if (wire)
    {
        wire->beginTransmission(0x68);
        if (wire->endTransmission() != 0)
            return false;
    }
    return maclock_local_rtc_present() &&
           !maclock_local_rtc_is_ds1307();
}

DateTime RTC_DS3231::now() const
{
    return DateTime(maclock_local_rtc_epoch());
}

void RTC_DS3231::adjust(const DateTime &date_time)
{
    maclock_local_rtc_adjust(date_time.unixtime());
}

bool RTC_DS3231::lostPower() const
{
    return false;
}

bool Adafruit_BMP5xx::begin(
    uint8_t address, TwoWire *wire)
{
    address_ = address;
    if (wire)
    {
        wire->beginTransmission(address);
        if (wire->endTransmission() != 0)
            return false;
    }
    return maclock_local_weather_sensor() ==
               MaclockLocalWeatherSensor::Bmp5xx &&
           maclock_local_weather_address() == address;
}

bool Adafruit_BMP5xx::performReading()
{
    if (maclock_local_weather_sensor() !=
            MaclockLocalWeatherSensor::Bmp5xx ||
        maclock_local_weather_address() != address_)
    {
        return false;
    }
    temperature = maclock_local_temperature();
    pressure = maclock_local_pressure();
    return true;
}

void Adafruit_BMP5xx::setTemperatureOversampling(int)
{
}

void Adafruit_BMP5xx::setPressureOversampling(int)
{
}

void Adafruit_BMP5xx::setIIRFilterCoeff(int)
{
}

void Adafruit_BMP5xx::setOutputDataRate(int)
{
}

void Adafruit_BMP5xx::setPowerMode(int)
{
}

void Adafruit_BMP5xx::enablePressure(bool)
{
}

bool Adafruit_HTU21DF::begin(TwoWire *wire)
{
    if (wire)
    {
        wire->beginTransmission(0x40);
        if (wire->endTransmission() != 0)
            return false;
    }
    return maclock_local_weather_sensor() ==
           MaclockLocalWeatherSensor::Htu2x;
}

float Adafruit_HTU21DF::readTemperature()
{
    return maclock_local_temperature();
}

float Adafruit_HTU21DF::readHumidity()
{
    return maclock_local_humidity();
}
