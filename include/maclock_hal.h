#pragma once

#include <stddef.h>
#include <stdint.h>

enum class MaclockPinMode : uint8_t
{
    Input,
    InputPullup,
    InputPulldown,
    Output
};

class MaclockRuntimeHal
{
public:
    virtual ~MaclockRuntimeHal() = default;
    virtual uint32_t millis() const = 0;
    virtual uint64_t micros() const = 0;
    virtual void delay(uint32_t milliseconds) = 0;
    virtual void yield() = 0;
};

class MaclockGpioHal
{
public:
    virtual ~MaclockGpioHal() = default;
    virtual void pinMode(int pin, MaclockPinMode mode) = 0;
    virtual int digitalRead(int pin) const = 0;
    virtual void digitalWrite(int pin, int value) = 0;
    virtual void analogWrite(int pin, int value) = 0;
    virtual int analogRead(int pin) const = 0;
};

class MaclockI2cHal
{
public:
    virtual ~MaclockI2cHal() = default;
    virtual void begin(int sda, int scl, uint32_t frequency) = 0;
    virtual void setClock(uint32_t frequency) = 0;
    virtual void beginTransmission(uint8_t address) = 0;
    virtual size_t write(uint8_t value) = 0;
    virtual uint8_t endTransmission(bool send_stop) = 0;
    virtual size_t requestFrom(uint8_t address, size_t count) = 0;
    virtual int available() const = 0;
    virtual int read() = 0;
};

class MaclockSpiHal
{
public:
    virtual ~MaclockSpiHal() = default;
    virtual void beginBus() = 0;
    virtual void beginTransaction(uint32_t frequency) = 0;
    virtual uint8_t transfer(uint8_t value) = 0;
    virtual void endTransaction() = 0;
};

class MaclockDisplayHal
{
public:
    virtual ~MaclockDisplayHal() = default;
    virtual void initialize(int width, int height) = 0;
    virtual void setRotation(uint8_t rotation) = 0;
    virtual uint8_t rotation() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void setAddressWindow(
        int x, int y, int width, int height) = 0;
    virtual void writePixels(
        const uint16_t *pixels, size_t count, bool swap_bytes) = 0;
    virtual void fill(uint16_t color) = 0;
    virtual void fillRect(
        int x, int y, int width, int height, uint16_t color) = 0;
    virtual void drawRect(
        int x, int y, int width, int height, uint16_t color) = 0;
};

class MaclockAudioHal
{
public:
    virtual ~MaclockAudioHal() = default;
    virtual bool begin(
        uint32_t sample_rate, uint8_t channels) = 0;
    virtual void stop() = 0;
    virtual size_t write(
        const int16_t *samples, size_t frame_count,
        uint8_t channels) = 0;
    virtual void drain() = 0;
    virtual void setVolume(uint8_t volume) = 0;
    virtual void setMuted(bool muted) = 0;
};

class MaclockStorageHal
{
public:
    virtual ~MaclockStorageHal() = default;
    virtual const char *dataDirectory() const = 0;
    virtual const char *stateDirectory() const = 0;
};

class MaclockNetworkHal
{
public:
    virtual ~MaclockNetworkHal() = default;
    virtual uint16_t remapServerPort(uint16_t requested) const = 0;
    virtual const char *simulatedSsid() const = 0;
};

class MaclockHal
{
public:
    virtual ~MaclockHal() = default;

    virtual bool begin() = 0;
    virtual void pump() = 0;
    virtual bool shouldQuit() const = 0;
    virtual void appReady() = 0;
    virtual bool overrideBootEmulator(bool &enabled) const
    {
        (void)enabled;
        return false;
    }

    virtual MaclockRuntimeHal &runtime() = 0;
    virtual MaclockGpioHal &gpio() = 0;
    virtual MaclockI2cHal &i2c() = 0;
    virtual MaclockSpiHal &spi() = 0;
    virtual MaclockDisplayHal &display() = 0;
    virtual MaclockAudioHal &audio() = 0;
    virtual MaclockStorageHal &storage() = 0;
    virtual MaclockNetworkHal &network() = 0;

    virtual bool isLocal() const { return false; }
};

void maclock_install_hal(MaclockHal &hal);
MaclockHal &maclock_hal();
bool maclock_hal_installed();
