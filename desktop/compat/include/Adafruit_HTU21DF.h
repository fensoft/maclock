#pragma once

#include <Wire.h>

#define HTU21DF_I2CADDR 0x40

class Adafruit_HTU21DF
{
public:
    bool begin(TwoWire *wire = &Wire);
    float readTemperature();
    float readHumidity();
};
