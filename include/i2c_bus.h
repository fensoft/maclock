#pragma once

#include <Wire.h>

class I2cBus
{
public:
    explicit I2cBus(TwoWire &wire = Wire) : wire_(wire) {}

    void begin(int sda, int scl, uint32_t frequency = 100000);
    bool present(uint8_t address);
    bool readRegister(uint8_t address, uint8_t reg, uint8_t &value);
    bool writeRegister(uint8_t address, uint8_t reg, uint8_t value);
    TwoWire &wire();

private:
    TwoWire &wire_;
};
