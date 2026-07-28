#pragma once

#include <Wire.h>

#define BMP5XX_ALTERNATIVE_ADDRESS 0x47
#define BMP5XX_DEFAULT_ADDRESS 0x46
#define BMP5XX_OVERSAMPLING_16X 0
#define BMP5XX_IIR_FILTER_COEFF_127 0
#define BMP5XX_ODR_120_HZ 0
#define BMP5XX_POWERMODE_NORMAL 0

class Adafruit_BMP5xx
{
public:
    bool begin(uint8_t address, TwoWire *wire = &Wire);
    bool performReading();
    void setTemperatureOversampling(int value);
    void setPressureOversampling(int value);
    void setIIRFilterCoeff(int value);
    void setOutputDataRate(int value);
    void setPowerMode(int value);
    void enablePressure(bool enabled);

    float temperature = 0.0f;
    float pressure = 0.0f;

private:
    uint8_t address_ = 0;
};
