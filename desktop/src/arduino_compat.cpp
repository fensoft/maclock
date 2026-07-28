#include <Arduino.h>
#include <ESP32Encoder.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <driver/i2s_std.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#include "es8311.h"
#include "local_maclock_hal.h"
#include "maclock_local_bridge.h"

#include <chrono>
#include <cstdio>
#include <thread>

HardwareSerial Serial;
TwoWire Wire;
SPIClass SPI;

bool String::equalsIgnoreCase(const String &other) const
{
    if (length() != other.length())
        return false;
    for (size_t i = 0; i < length(); ++i)
    {
        const unsigned char a =
            static_cast<unsigned char>((*this)[i]);
        const unsigned char b =
            static_cast<unsigned char>(other[i]);
        if (std::tolower(a) != std::tolower(b))
            return false;
    }
    return true;
}

void String::trim()
{
    const auto first = value_.find_first_not_of(
        " \t\r\n");
    if (first == std::string::npos)
    {
        value_.clear();
        read_position_ = 0;
        return;
    }
    const auto last = value_.find_last_not_of(
        " \t\r\n");
    value_ = value_.substr(first, last - first + 1);
    read_position_ = 0;
}

void HardwareSerial::begin(unsigned long)
{
}

void HardwareSerial::print(const char *value)
{
    std::fputs(value ? value : "", stdout);
    std::fflush(stdout);
}

void HardwareSerial::print(const String &value)
{
    print(value.c_str());
}

void HardwareSerial::print(char value)
{
    std::fputc(value, stdout);
    std::fflush(stdout);
}

void HardwareSerial::print(int value)
{
    std::printf("%d", value);
    std::fflush(stdout);
}

void HardwareSerial::println()
{
    print("\n");
}

void HardwareSerial::println(const char *value)
{
    print(value);
    print("\n");
}

void HardwareSerial::println(const String &value)
{
    println(value.c_str());
}

int HardwareSerial::printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const int result = std::vprintf(format, args);
    va_end(args);
    std::fflush(stdout);
    return result;
}

uint32_t millis()
{
    return maclock_hal().runtime().millis();
}

uint32_t micros()
{
    return static_cast<uint32_t>(
        maclock_hal().runtime().micros());
}

void delay(uint32_t milliseconds)
{
    maclock_hal().runtime().delay(milliseconds);
}

void delayMicroseconds(uint32_t microseconds)
{
    if (microseconds >= 1000)
    {
        maclock_hal().runtime().delay(
            microseconds / 1000);
        return;
    }
    std::this_thread::sleep_for(
        std::chrono::microseconds(microseconds));
}

void yield()
{
    maclock_hal().runtime().yield();
}

namespace
{
MaclockPinMode convert_mode(int mode)
{
    switch (mode)
    {
    case OUTPUT:
        return MaclockPinMode::Output;
    case INPUT_PULLUP:
        return MaclockPinMode::InputPullup;
    case INPUT_PULLDOWN:
        return MaclockPinMode::InputPulldown;
    default:
        return MaclockPinMode::Input;
    }
}
}

void pinMode(int pin, int mode)
{
    maclock_hal().gpio().pinMode(pin, convert_mode(mode));
}

int digitalRead(int pin)
{
    return maclock_hal().gpio().digitalRead(pin);
}

void digitalWrite(int pin, int value)
{
    maclock_hal().gpio().digitalWrite(pin, value);
}

void analogWrite(int pin, int value)
{
    maclock_hal().gpio().analogWrite(pin, value);
}

int analogRead(int pin)
{
    return maclock_hal().gpio().analogRead(pin);
}

uint32_t touchRead(int pin)
{
    return digitalRead(pin) == LOW ? 125000 : 100000;
}

long random(long maximum)
{
    return maximum > 0 ? std::rand() % maximum : 0;
}

long random(long minimum, long maximum)
{
    return minimum +
           random(std::max<long>(0, maximum - minimum));
}

long map(
    long value, long from_low, long from_high,
    long to_low, long to_high)
{
    if (from_high == from_low)
        return to_low;
    return (value - from_low) *
               (to_high - to_low) /
               (from_high - from_low) +
           to_low;
}

void configTime(
    long, int, const char *, const char *, const char *)
{
}

bool getLocalTime(struct tm *time_info, uint32_t)
{
    if (!time_info)
        return false;
    const std::time_t now = std::time(nullptr);
    return localtime_r(&now, time_info) != nullptr;
}

size_t strlcpy(
    char *destination, const char *source, size_t size)
{
    const size_t length = std::strlen(source ? source : "");
    if (size)
    {
        const size_t copied = std::min(length, size - 1);
        std::memcpy(destination, source ? source : "", copied);
        destination[copied] = '\0';
    }
    return length;
}

bool TwoWire::begin(int sda, int scl)
{
    maclock_hal().i2c().begin(sda, scl, 100000);
    return true;
}

void TwoWire::setClock(uint32_t frequency)
{
    maclock_hal().i2c().setClock(frequency);
}

void TwoWire::beginTransmission(uint8_t address)
{
    maclock_hal().i2c().beginTransmission(address);
}

size_t TwoWire::write(uint8_t value)
{
    return maclock_hal().i2c().write(value);
}

size_t TwoWire::write(
    const uint8_t *values, size_t count)
{
    size_t written = 0;
    for (size_t i = 0; values && i < count; ++i)
        written += write(values[i]);
    return written;
}

uint8_t TwoWire::endTransmission(bool send_stop)
{
    return maclock_hal().i2c().endTransmission(send_stop);
}

size_t TwoWire::requestFrom(
    uint8_t address, uint8_t count)
{
    return maclock_hal().i2c().requestFrom(address, count);
}

int TwoWire::available()
{
    return maclock_hal().i2c().available();
}

int TwoWire::read()
{
    return maclock_hal().i2c().read();
}

void SPIClass::begin()
{
    maclock_hal().spi().beginBus();
}

void SPIClass::beginTransaction(const SPISettings &settings)
{
    maclock_hal().spi().beginTransaction(
        settings.frequency());
}

uint8_t SPIClass::transfer(uint8_t value)
{
    return maclock_hal().spi().transfer(value);
}

void SPIClass::endTransaction()
{
    maclock_hal().spi().endTransaction();
}

void TFT_eSPI::init()
{
    maclock_hal().display().initialize(LCD_W, LCD_H);
}

void TFT_eSPI::setRotation(uint8_t rotation)
{
    maclock_hal().display().setRotation(rotation);
}

uint8_t TFT_eSPI::getRotation() const
{
    return maclock_hal().display().rotation();
}

int16_t TFT_eSPI::width() const
{
    return static_cast<int16_t>(
        maclock_hal().display().width());
}

int16_t TFT_eSPI::height() const
{
    return static_cast<int16_t>(
        maclock_hal().display().height());
}

void TFT_eSPI::setAddrWindow(
    int32_t x, int32_t y, int32_t width, int32_t height)
{
    maclock_hal().display().setAddressWindow(
        x, y, width, height);
}

void TFT_eSPI::pushColors(
    uint16_t *pixels, uint32_t count, bool swap_bytes)
{
    pushColors(
        static_cast<const uint16_t *>(pixels),
        count, swap_bytes);
}

void TFT_eSPI::pushColors(
    const uint16_t *pixels, uint32_t count, bool swap_bytes)
{
    maclock_hal().display().writePixels(
        pixels, count, swap_bytes);
}

void TFT_eSPI::fillScreen(uint16_t color)
{
    maclock_hal().display().fill(color);
}

void TFT_eSPI::fillRect(
    int32_t x, int32_t y, int32_t width, int32_t height,
    uint16_t color)
{
    maclock_hal().display().fillRect(
        x, y, width, height, color);
}

void TFT_eSPI::drawRect(
    int32_t x, int32_t y, int32_t width, int32_t height,
    uint16_t color)
{
    maclock_hal().display().drawRect(
        x, y, width, height, color);
}

void TFT_eSPI::startWrite()
{
    SPI.beginTransaction(
        SPISettings(40000000, MSBFIRST, SPI_MODE0));
}

void TFT_eSPI::endWrite()
{
    SPI.endTransaction();
}

void TFT_eSPI::setTextColor(
    uint16_t foreground, uint16_t background)
{
    text_color_ = foreground;
    text_background_ = background;
}

void TFT_eSPI::setTextSize(uint8_t size)
{
    text_size_ = std::max<uint8_t>(1, size);
}

int16_t TFT_eSPI::drawString(
    const String &text, int32_t x, int32_t y, uint8_t)
{
    const int cell_width = 4 * text_size_;
    const int cell_height = 6 * text_size_;
    fillRect(
        x, y,
        static_cast<int>(text.length()) * cell_width,
        cell_height, text_background_);
    for (size_t i = 0; i < text.length(); ++i)
    {
        if (text[i] != ' ')
        {
            drawRect(
                x + static_cast<int>(i) * cell_width,
                y, cell_width - 1, cell_height - 1,
                text_color_);
        }
    }
    return static_cast<int16_t>(
        text.length() * cell_width);
}

void ESP32Encoder::attachHalfQuad(int, int)
{
    maclock_local_register_encoder(this);
}

int64_t ESP32Encoder::getCount() const
{
    return count_.load();
}

void ESP32Encoder::setCount(int64_t count)
{
    count_.store(count);
}

void ESP32Encoder::localAdd(int64_t delta)
{
    count_.fetch_add(delta);
}

void *heap_caps_malloc(size_t size, unsigned)
{
    return std::malloc(size);
}

void *heap_caps_calloc(
    size_t count, size_t size, unsigned)
{
    return std::calloc(count, size);
}

void *heap_caps_aligned_alloc(
    size_t alignment, size_t size, unsigned)
{
    void *result = nullptr;
    return posix_memalign(&result, alignment, size) == 0
               ? result
               : nullptr;
}

void heap_caps_free(void *memory)
{
    std::free(memory);
}

void heap_caps_malloc_extmem_enable(size_t)
{
}

int64_t esp_timer_get_time()
{
    return static_cast<int64_t>(
        maclock_hal().runtime().micros());
}

int i2s_channel_write(
    i2s_chan_handle_t,
    const void *source,
    size_t bytes,
    size_t *bytes_written,
    uint32_t)
{
    const size_t frame_count =
        bytes / (2 * sizeof(int16_t));
    const size_t written = maclock_hal().audio().write(
        static_cast<const int16_t *>(source),
        frame_count, 2);
    if (bytes_written)
        *bytes_written = written * 2 * sizeof(int16_t);
    return ESP_OK;
}

namespace
{
struct LocalCodec
{
    TwoWire *bus = nullptr;
    uint8_t address = ES8311_ADDRESS_0;
    uint8_t volume = 0;
    bool muted = true;
};

void codecWrite(
    LocalCodec *codec, uint8_t reg, uint8_t value)
{
    if (!codec || !codec->bus)
        return;
    codec->bus->beginTransmission(codec->address);
    codec->bus->write(reg);
    codec->bus->write(value);
    codec->bus->endTransmission();
}
}

extern "C"
{
es8311_handle_t es8311_create(
    const es8311_i2c_t bus, const uint16_t address)
{
    auto *codec = new LocalCodec();
    codec->bus = bus;
    codec->address = static_cast<uint8_t>(address);
    if (bus)
    {
        bus->beginTransmission(codec->address);
        if (bus->endTransmission() != 0)
        {
            delete codec;
            return nullptr;
        }
    }
    return codec;
}

void es8311_delete(es8311_handle_t device)
{
    delete static_cast<LocalCodec *>(device);
}

esp_err_t es8311_init(
    es8311_handle_t device,
    const es8311_clock_config_t *clock,
    const es8311_resolution_t,
    const es8311_resolution_t)
{
    codecWrite(
        static_cast<LocalCodec *>(device), 0x00, 0x80);
    if (clock)
        maclock_local_audio_rate(clock->sample_frequency);
    return ESP_OK;
}

esp_err_t es8311_voice_volume_set(
    es8311_handle_t device, int volume, int *volume_set)
{
    volume = std::clamp(volume, 0, 100);
    if (device)
    {
        auto *codec = static_cast<LocalCodec *>(device);
        codec->volume = static_cast<uint8_t>(volume);
        codecWrite(
            codec, 0x32,
            static_cast<uint8_t>(volume * 255 / 100));
    }
    maclock_hal().audio().setVolume(
        static_cast<uint8_t>(volume));
    if (volume_set)
        *volume_set = volume;
    return ESP_OK;
}

esp_err_t es8311_voice_volume_get(
    es8311_handle_t device, int *volume)
{
    if (!device || !volume)
        return ESP_ERR_INVALID_ARG;
    *volume = static_cast<LocalCodec *>(device)->volume;
    return ESP_OK;
}

esp_err_t es8311_voice_mute(
    es8311_handle_t device, bool muted)
{
    if (device)
    {
        auto *codec = static_cast<LocalCodec *>(device);
        codec->muted = muted;
        codecWrite(codec, 0x31, muted ? 0x60 : 0x00);
    }
    maclock_hal().audio().setMuted(muted);
    return ESP_OK;
}

esp_err_t es8311_sample_frequency_config(
    es8311_handle_t, int, int sample_frequency)
{
    maclock_local_audio_rate(sample_frequency);
    return ESP_OK;
}

esp_err_t es8311_voice_fade(
    es8311_handle_t, const es8311_fade_t)
{
    return ESP_OK;
}

esp_err_t es8311_microphone_fade(
    es8311_handle_t, const es8311_fade_t)
{
    return ESP_OK;
}

esp_err_t es8311_microphone_gain_set(
    es8311_handle_t, es8311_mic_gain_t)
{
    return ESP_OK;
}

esp_err_t es8311_microphone_config(
    es8311_handle_t, bool)
{
    return ESP_OK;
}

void es8311_register_dump(es8311_handle_t)
{
}

esp_err_t es8311_codec_init(void)
{
    return ESP_OK;
}
}

bool maclock_local_rtc_present()
{
    return local_maclock_hal().rtcPresent();
}

bool maclock_local_rtc_is_ds1307()
{
    return local_maclock_hal().rtcIsDs1307();
}

uint32_t maclock_local_rtc_epoch()
{
    return local_maclock_hal().rtcEpoch();
}

void maclock_local_rtc_adjust(uint32_t epoch)
{
    local_maclock_hal().adjustRtc(epoch);
}

void maclock_local_rtc_reset()
{
    local_maclock_hal().resetRtc();
}

MaclockLocalWeatherSensor maclock_local_weather_sensor()
{
    switch (local_maclock_hal().weatherKind())
    {
    case 0:
        return MaclockLocalWeatherSensor::Bmp5xx;
    case 1:
        return MaclockLocalWeatherSensor::Htu2x;
    default:
        return MaclockLocalWeatherSensor::None;
    }
}

uint8_t maclock_local_weather_address()
{
    return local_maclock_hal().weatherAddress();
}

float maclock_local_temperature()
{
    return local_maclock_hal().temperature();
}

float maclock_local_pressure()
{
    return local_maclock_hal().pressure();
}

float maclock_local_humidity()
{
    return local_maclock_hal().humidity();
}

void maclock_local_register_encoder(ESP32Encoder *encoder)
{
    local_maclock_hal().registerEncoder(encoder);
}

void maclock_local_audio_rate(uint32_t sample_rate)
{
    maclock_hal().audio().begin(sample_rate, 2);
}
