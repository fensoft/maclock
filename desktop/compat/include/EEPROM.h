#pragma once

#include <stddef.h>
#include <stdint.h>
#include <cstring>

class EEPROMClass
{
public:
    bool begin(size_t size);
    bool commit();
    uint8_t read(int address) const;
    void write(int address, uint8_t value);

    template <typename T>
    T &get(int address, T &value) const
    {
        readBlock(address, &value, sizeof(value));
        return value;
    }

    template <typename T>
    const T &put(int address, const T &value)
    {
        writeBlock(address, &value, sizeof(value));
        return value;
    }

private:
    void readBlock(int address, void *value, size_t size) const;
    void writeBlock(int address, const void *value, size_t size);
};

extern EEPROMClass EEPROM;
