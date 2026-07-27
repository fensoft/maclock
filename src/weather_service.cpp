#include "weather_service.h"

#include <Arduino.h>
#include <math.h>

void WeatherService::begin()
{
    type_ = WeatherSensorType::None;
    address_ = 0;
    if (bmp_.begin(BMP5XX_ALTERNATIVE_ADDRESS, &bus_.wire()))
    {
        type_ = WeatherSensorType::Bmp5xx;
        address_ = BMP5XX_ALTERNATIVE_ADDRESS;
        bmp_.setTemperatureOversampling(BMP5XX_OVERSAMPLING_16X);
        bmp_.setPressureOversampling(BMP5XX_OVERSAMPLING_16X);
        bmp_.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_127);
        bmp_.setOutputDataRate(BMP5XX_ODR_120_HZ);
        bmp_.setPowerMode(BMP5XX_POWERMODE_NORMAL);
        bmp_.enablePressure(true);
        Serial.println("BMP5xx detected at 0x47");
        return;
    }

    if (htu2x_.begin(&bus_.wire()))
    {
        type_ = WeatherSensorType::Htu2x;
        address_ = HTU21DF_I2CADDR;
        Serial.println("HTU2x detected at 0x40");
        return;
    }
    Serial.println("No weather sensor detected");
}

WeatherReading WeatherService::read()
{
    WeatherReading reading;
    switch (type_)
    {
    case WeatherSensorType::Bmp5xx:
        if (!bmp_.performReading())
            return reading;
        reading.valid = true;
        reading.temperature = bmp_.temperature;
        reading.gauge_value = bmp_.pressure;
        reading.gauge_min = 980.0f;
        reading.gauge_max = 1040.0f;
        reading.condition =
            reading.gauge_value < 1000.0f
                ? WeatherCondition::Rainy
                : reading.gauge_value < 1020.0f
                      ? WeatherCondition::Cloudy
                      : WeatherCondition::Sunny;
        return reading;

    case WeatherSensorType::Htu2x:
    {
        const float temperature = htu2x_.readTemperature();
        const float humidity = htu2x_.readHumidity();
        if (isnan(temperature) || isnan(humidity))
            return reading;
        reading.valid = true;
        reading.temperature = temperature;
        reading.gauge_value = humidity;
        reading.gauge_min = 0.0f;
        reading.gauge_max = 100.0f;
        reading.condition =
            humidity >= 70.0f
                ? WeatherCondition::Rainy
                : humidity >= 40.0f
                      ? WeatherCondition::Cloudy
                      : WeatherCondition::Sunny;
        return reading;
    }

    default:
        return reading;
    }
}

WeatherSensorType WeatherService::type() const
{
    return type_;
}

uint8_t WeatherService::address() const
{
    return address_;
}
