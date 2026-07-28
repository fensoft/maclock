#pragma once

#include <Arduino.h>

class TwoWire
{
public:
    bool begin(int sda = -1, int scl = -1);
    void setClock(uint32_t frequency);
    void beginTransmission(uint8_t address);
    size_t write(uint8_t value);
    size_t write(const uint8_t *values, size_t count);
    uint8_t endTransmission(bool send_stop = true);
    size_t requestFrom(uint8_t address, uint8_t count);
    int available();
    int read();
};

extern TwoWire Wire;
