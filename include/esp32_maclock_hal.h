#pragma once

#include "maclock_hal.h"

class Esp32MaclockHal final
    : public MaclockHal,
      private MaclockRuntimeHal,
      private MaclockGpioHal,
      private MaclockI2cHal,
      private MaclockSpiHal,
      private MaclockDisplayHal,
      private MaclockAudioHal,
      private MaclockStorageHal,
      private MaclockNetworkHal
{
public:
    bool begin() override;
    void pump() override;
    bool shouldQuit() const override;
    void appReady() override;

    MaclockRuntimeHal &runtime() override { return *this; }
    MaclockGpioHal &gpio() override { return *this; }
    MaclockI2cHal &i2c() override { return *this; }
    MaclockSpiHal &spi() override { return *this; }
    MaclockDisplayHal &display() override { return *this; }
    MaclockAudioHal &audio() override { return *this; }
    MaclockStorageHal &storage() override { return *this; }
    MaclockNetworkHal &network() override { return *this; }

private:
    uint32_t millis() const override;
    uint64_t micros() const override;
    void delay(uint32_t milliseconds) override;
    void yield() override;

    void pinMode(int pin, MaclockPinMode mode) override;
    int digitalRead(int pin) const override;
    void digitalWrite(int pin, int value) override;
    void analogWrite(int pin, int value) override;
    int analogRead(int pin) const override;

    void begin(int sda, int scl, uint32_t frequency) override;
    void setClock(uint32_t frequency) override;
    void beginTransmission(uint8_t address) override;
    size_t write(uint8_t value) override;
    uint8_t endTransmission(bool send_stop) override;
    size_t requestFrom(uint8_t address, size_t count) override;
    int available() const override;
    int read() override;

    void beginBus() override;
    void beginTransaction(uint32_t frequency) override;
    uint8_t transfer(uint8_t value) override;
    void endTransaction() override;

    void initialize(int width, int height) override;
    void setRotation(uint8_t rotation) override;
    uint8_t rotation() const override;
    int width() const override;
    int height() const override;
    void setAddressWindow(
        int x, int y, int width, int height) override;
    void writePixels(
        const uint16_t *pixels, size_t count, bool swap_bytes) override;
    void fill(uint16_t color) override;
    void fillRect(
        int x, int y, int width, int height, uint16_t color) override;
    void drawRect(
        int x, int y, int width, int height, uint16_t color) override;

    bool begin(uint32_t sample_rate, uint8_t channels) override;
    void stop() override;
    size_t write(
        const int16_t *samples, size_t frame_count,
        uint8_t channels) override;
    void drain() override;
    void setVolume(uint8_t volume) override;
    void setMuted(bool muted) override;

    const char *dataDirectory() const override;
    const char *stateDirectory() const override;
    uint16_t remapServerPort(uint16_t requested) const override;
    const char *simulatedSsid() const override;
};
