#pragma once

#include <Arduino.h>
#include <Wire.h>

class DateTime
{
public:
    DateTime();
    explicit DateTime(uint32_t epoch);
    DateTime(
        uint16_t year, uint8_t month, uint8_t day,
        uint8_t hour = 0, uint8_t minute = 0,
        uint8_t second = 0);

    uint16_t year() const { return year_; }
    uint8_t month() const { return month_; }
    uint8_t day() const { return day_; }
    uint8_t hour() const { return hour_; }
    uint8_t minute() const { return minute_; }
    uint8_t second() const { return second_; }
    uint8_t dayOfTheWeek() const;
    uint32_t unixtime() const;
    bool isValid() const;

private:
    uint16_t year_ = 2000;
    uint8_t month_ = 1;
    uint8_t day_ = 1;
    uint8_t hour_ = 0;
    uint8_t minute_ = 0;
    uint8_t second_ = 0;
};

class RTC_DS1307
{
public:
    bool begin(TwoWire *wire = &Wire);
    DateTime now() const;
    void adjust(const DateTime &date_time);
    bool isrunning() const;
};

class RTC_DS3231
{
public:
    bool begin(TwoWire *wire = &Wire);
    DateTime now() const;
    void adjust(const DateTime &date_time);
    bool lostPower() const;
};
