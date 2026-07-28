#include "esp32_maclock_hal.h"

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

bool Esp32MaclockHal::begin()
{
    return true;
}

void Esp32MaclockHal::pump()
{
}

bool Esp32MaclockHal::shouldQuit() const
{
    return false;
}

void Esp32MaclockHal::appReady()
{
}

uint32_t Esp32MaclockHal::millis() const
{
    return ::millis();
}

uint64_t Esp32MaclockHal::micros() const
{
    return ::micros();
}

void Esp32MaclockHal::delay(uint32_t milliseconds)
{
    ::delay(milliseconds);
}

void Esp32MaclockHal::yield()
{
    ::yield();
}

void Esp32MaclockHal::pinMode(int pin, MaclockPinMode mode)
{
    uint8_t native_mode = INPUT;
    switch (mode)
    {
    case MaclockPinMode::InputPullup:
        native_mode = INPUT_PULLUP;
        break;
    case MaclockPinMode::InputPulldown:
        native_mode = INPUT_PULLDOWN;
        break;
    case MaclockPinMode::Output:
        native_mode = OUTPUT;
        break;
    default:
        break;
    }
    ::pinMode(pin, native_mode);
}

int Esp32MaclockHal::digitalRead(int pin) const
{
    return ::digitalRead(pin);
}

void Esp32MaclockHal::digitalWrite(int pin, int value)
{
    ::digitalWrite(pin, value);
}

void Esp32MaclockHal::analogWrite(int pin, int value)
{
    ::analogWrite(pin, value);
}

int Esp32MaclockHal::analogRead(int pin) const
{
    return ::analogRead(pin);
}

void Esp32MaclockHal::begin(
    int sda, int scl, uint32_t frequency)
{
    Wire.begin(sda, scl);
    Wire.setClock(frequency);
}

void Esp32MaclockHal::setClock(uint32_t frequency)
{
    Wire.setClock(frequency);
}

void Esp32MaclockHal::beginTransmission(uint8_t address)
{
    Wire.beginTransmission(address);
}

size_t Esp32MaclockHal::write(uint8_t value)
{
    return Wire.write(value);
}

uint8_t Esp32MaclockHal::endTransmission(bool send_stop)
{
    return Wire.endTransmission(send_stop);
}

size_t Esp32MaclockHal::requestFrom(
    uint8_t address, size_t count)
{
    return Wire.requestFrom(
        address, static_cast<uint8_t>(count));
}

int Esp32MaclockHal::available() const
{
    return Wire.available();
}

int Esp32MaclockHal::read()
{
    return Wire.read();
}

void Esp32MaclockHal::beginBus()
{
    SPI.begin();
}

void Esp32MaclockHal::beginTransaction(uint32_t frequency)
{
    SPI.beginTransaction(
        SPISettings(frequency, MSBFIRST, SPI_MODE0));
}

uint8_t Esp32MaclockHal::transfer(uint8_t value)
{
    return SPI.transfer(value);
}

void Esp32MaclockHal::endTransaction()
{
    SPI.endTransaction();
}

void Esp32MaclockHal::initialize(int width, int height)
{
    (void)width;
    (void)height;
}

void Esp32MaclockHal::setRotation(uint8_t rotation)
{
    (void)rotation;
}

uint8_t Esp32MaclockHal::rotation() const
{
    return 0;
}

int Esp32MaclockHal::width() const
{
    return LCD_W;
}

int Esp32MaclockHal::height() const
{
    return LCD_H;
}

void Esp32MaclockHal::setAddressWindow(
    int x, int y, int width, int height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

void Esp32MaclockHal::writePixels(
    const uint16_t *pixels, size_t count, bool swap_bytes)
{
    (void)pixels;
    (void)count;
    (void)swap_bytes;
}

void Esp32MaclockHal::fill(uint16_t color)
{
    (void)color;
}

void Esp32MaclockHal::fillRect(
    int x, int y, int width, int height, uint16_t color)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)color;
}

void Esp32MaclockHal::drawRect(
    int x, int y, int width, int height, uint16_t color)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)color;
}

bool Esp32MaclockHal::begin(
    uint32_t sample_rate, uint8_t channels)
{
    (void)sample_rate;
    (void)channels;
    return true;
}

void Esp32MaclockHal::stop()
{
}

size_t Esp32MaclockHal::write(
    const int16_t *samples, size_t frame_count,
    uint8_t channels)
{
    (void)samples;
    (void)channels;
    return frame_count;
}

void Esp32MaclockHal::drain()
{
}

void Esp32MaclockHal::setVolume(uint8_t volume)
{
    (void)volume;
}

void Esp32MaclockHal::setMuted(bool muted)
{
    (void)muted;
}

const char *Esp32MaclockHal::dataDirectory() const
{
    return "/";
}

const char *Esp32MaclockHal::stateDirectory() const
{
    return "/";
}

uint16_t Esp32MaclockHal::remapServerPort(
    uint16_t requested) const
{
    return requested;
}

const char *Esp32MaclockHal::simulatedSsid() const
{
    return "";
}
