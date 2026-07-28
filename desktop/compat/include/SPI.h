#pragma once

#include <Arduino.h>

#define MSBFIRST 1
#define SPI_MODE0 0

class SPISettings
{
public:
    SPISettings(
        uint32_t frequency = 1000000,
        uint8_t bit_order = MSBFIRST,
        uint8_t mode = SPI_MODE0)
        : frequency_(frequency),
          bit_order_(bit_order),
          mode_(mode)
    {
    }

    uint32_t frequency() const { return frequency_; }

private:
    uint32_t frequency_;
    uint8_t bit_order_;
    uint8_t mode_;
};

class SPIClass
{
public:
    void begin();
    void beginTransaction(const SPISettings &settings);
    uint8_t transfer(uint8_t value);
    void endTransaction();
};

extern SPIClass SPI;
