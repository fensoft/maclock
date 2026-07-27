#include "i2c_bus.h"

void I2cBus::begin(int sda, int scl, uint32_t frequency)
{
    wire_.begin(sda, scl);
    wire_.setClock(frequency);
}

bool I2cBus::present(uint8_t address)
{
    wire_.beginTransmission(address);
    return wire_.endTransmission() == 0;
}

bool I2cBus::readRegister(uint8_t address,
                          uint8_t reg,
                          uint8_t &value)
{
    wire_.beginTransmission(address);
    wire_.write(reg);
    if (wire_.endTransmission(false) != 0)
        return false;
    if (wire_.requestFrom(address, static_cast<uint8_t>(1)) != 1)
        return false;
    value = wire_.read();
    return true;
}

bool I2cBus::writeRegister(uint8_t address,
                           uint8_t reg,
                           uint8_t value)
{
    wire_.beginTransmission(address);
    wire_.write(reg);
    wire_.write(value);
    return wire_.endTransmission() == 0;
}

TwoWire &I2cBus::wire()
{
    return wire_;
}
