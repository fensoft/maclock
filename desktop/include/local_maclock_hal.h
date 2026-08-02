#pragma once

#include "maclock_hal.h"

#include <memory>
#include <string>

enum class LocalStartupMode : uint8_t
{
    Config,
    Clock,
    Emulator,
    Firmware
};

struct LocalMaclockOptions
{
    LocalStartupMode startup = LocalStartupMode::Config;
    std::string data_directory;
    std::string state_directory;
    uint16_t http_port = 8088;
    // Zero selects the largest integer scale that fits the usable display.
    uint8_t scale = 0;
    bool reset_state = false;
    bool floppy_inserted = false;
    bool touch_disconnected = false;
    bool touch_option_set = false;
    bool headless = false;
    uint32_t run_for_ms = 0;
    std::string framebuffer_output;
};

class LocalMaclockHal final
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
    explicit LocalMaclockHal(LocalMaclockOptions options);
    ~LocalMaclockHal() override;

    bool begin() override;
    void pump() override;
    bool shouldQuit() const override;
    void requestQuit() noexcept;
    bool restartRequested() const;
    LocalStartupMode restartStartup() const;
    bool touchscreenPresent() const;
    void appReady() override;
    void emulatorModeChanged(bool active) override;
    bool overrideBootEmulator(bool &enabled) const override;
    bool isLocal() const override { return true; }

    MaclockRuntimeHal &runtime() override { return *this; }
    MaclockGpioHal &gpio() override { return *this; }
    MaclockI2cHal &i2c() override { return *this; }
    MaclockSpiHal &spi() override { return *this; }
    MaclockDisplayHal &display() override { return *this; }
    MaclockAudioHal &audio() override { return *this; }
    MaclockStorageHal &storage() override { return *this; }
    MaclockNetworkHal &network() override { return *this; }

    void registerEncoder(void *encoder);
    bool rtcPresent() const;
    bool rtcIsDs1307() const;
    uint32_t rtcEpoch() const;
    void adjustRtc(uint32_t epoch);
    void resetRtc();
    uint8_t weatherKind() const;
    uint8_t weatherAddress() const;
    float temperature() const;
    float pressure() const;
    float humidity() const;
    bool saveFramebuffer(const std::string &path) const;

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
        const uint16_t *pixels, size_t count,
        bool swap_bytes) override;
    void fill(uint16_t color) override;
    void fillRect(
        int x, int y, int width, int height,
        uint16_t color) override;
    void drawRect(
        int x, int y, int width, int height,
        uint16_t color) override;

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

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

LocalMaclockHal &local_maclock_hal();
