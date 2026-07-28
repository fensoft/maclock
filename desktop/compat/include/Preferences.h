#pragma once

#include <Arduino.h>

class Preferences
{
public:
    bool begin(
        const char *name, bool read_only = false,
        const char *partition_label = nullptr);
    void end();
    bool isKey(const char *key) const;

    bool getBool(const char *key, bool fallback = false) const;
    uint8_t getUChar(
        const char *key, uint8_t fallback = 0) const;
    uint16_t getUShort(
        const char *key, uint16_t fallback = 0) const;
    int32_t getInt(
        const char *key, int32_t fallback = 0) const;
    double getDouble(
        const char *key, double fallback = 0.0) const;
    String getString(
        const char *key, const String &fallback = String()) const;
    size_t getBytesLength(const char *key) const;
    size_t getBytes(
        const char *key, void *buffer, size_t maximum) const;

    size_t putBool(const char *key, bool value);
    size_t putUChar(const char *key, uint8_t value);
    size_t putUShort(const char *key, uint16_t value);
    size_t putInt(const char *key, int32_t value);
    size_t putDouble(const char *key, double value);
    size_t putString(const char *key, const String &value);
    size_t putString(const char *key, const char *value);
    size_t putBytes(
        const char *key, const void *value, size_t length);

private:
    String namespace_;
    bool read_only_ = false;
};
