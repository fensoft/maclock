#pragma once

#include <math.h>

#include <Adafruit_BMP5xx.h>
#include <Adafruit_HTU21DF.h>

#include "i2c_bus.h"

enum class WeatherSensorType : uint8_t
{
    None,
    Bmp5xx,
    Htu2x
};

enum class WeatherCondition : uint8_t
{
    Sunny,
    Cloudy,
    Rainy
};

struct WeatherReading
{
    bool valid = false;
    float temperature = NAN;
    float gauge_value = 0.0f;
    float gauge_min = 0.0f;
    float gauge_max = 1.0f;
    WeatherCondition condition = WeatherCondition::Cloudy;
};

class WeatherService
{
public:
    explicit WeatherService(I2cBus &bus) : bus_(bus) {}

    void begin();
    WeatherReading read();

    WeatherSensorType type() const;
    uint8_t address() const;

private:
    I2cBus &bus_;
    Adafruit_BMP5xx bmp_;
    Adafruit_HTU21DF htu2x_;
    WeatherSensorType type_ = WeatherSensorType::None;
    uint8_t address_ = 0;
};
