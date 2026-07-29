#pragma once

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3
#define PROGMEM
#define IRAM_ATTR
#define ARDUINO_ISR_ATTR
#define F(value) value

using byte = uint8_t;
using boolean = bool;
using PGM_P = const char *;

class String
{
public:
    String() = default;
    String(const char *value) : value_(value ? value : "") {}
    String(const std::string &value) : value_(value) {}
    String(char value) : value_(1, value) {}
    String(int value) : value_(std::to_string(value)) {}
    String(unsigned value) : value_(std::to_string(value)) {}
    String(long value) : value_(std::to_string(value)) {}
    String(unsigned long value) : value_(std::to_string(value)) {}
    String(double value) : value_(std::to_string(value)) {}

    size_t length() const { return value_.size(); }
    bool isEmpty() const { return value_.empty(); }
    const char *c_str() const { return value_.c_str(); }
    void reserve(size_t size) { value_.reserve(size); }
    void clear() { value_.clear(); }
    size_t write(uint8_t value)
    {
        value_.push_back(static_cast<char>(value));
        return 1;
    }
    size_t write(const uint8_t *values, size_t count)
    {
        if (values && count)
        {
            value_.append(
                reinterpret_cast<const char *>(values), count);
        }
        return count;
    }

    String substring(size_t from, size_t to) const
    {
        from = std::min(from, value_.size());
        to = std::min(std::max(to, from), value_.size());
        return value_.substr(from, to - from);
    }

    String substring(size_t from) const
    {
        return substring(from, value_.size());
    }

    bool startsWith(const char *prefix) const
    {
        if (!prefix)
            return false;
        const size_t prefix_length = std::strlen(prefix);
        return prefix_length <= value_.size() &&
               value_.compare(0, prefix_length, prefix) == 0;
    }

    int lastIndexOf(const char *value) const
    {
        const size_t position =
            value_.rfind(value ? value : "");
        return position == std::string::npos
                   ? -1
                   : static_cast<int>(position);
    }

    void toCharArray(char *buffer, size_t size) const
    {
        if (!buffer || size == 0)
            return;
        std::strncpy(buffer, value_.c_str(), size - 1);
        buffer[size - 1] = '\0';
    }

    bool equalsIgnoreCase(const String &other) const;
    void trim();
    void toUpperCase()
    {
        std::transform(
            value_.begin(), value_.end(), value_.begin(),
            [](unsigned char value)
            {
                return static_cast<char>(std::toupper(value));
            });
    }
    int read() const
    {
        return read_position_ < value_.size()
                   ? static_cast<unsigned char>(
                         value_[read_position_++])
                   : -1;
    }

    String &operator+=(const String &other)
    {
        value_ += other.value_;
        return *this;
    }
    String &operator+=(const char *other)
    {
        value_ += other ? other : "";
        return *this;
    }
    String &operator+=(char value)
    {
        value_ += value;
        return *this;
    }

    char operator[](size_t index) const { return value_[index]; }
    auto begin() const { return value_.begin(); }
    auto end() const { return value_.end(); }
    const std::string &stdString() const { return value_; }

    friend bool operator==(const String &a, const String &b)
    {
        return a.value_ == b.value_;
    }
    friend bool operator!=(const String &a, const String &b)
    {
        return !(a == b);
    }
    friend bool operator==(const String &a, const char *b)
    {
        return a.value_ == (b ? b : "");
    }
    friend bool operator==(const char *a, const String &b)
    {
        return b == a;
    }
    friend bool operator!=(const String &a, const char *b)
    {
        return !(a == b);
    }
    friend String operator+(String a, const String &b)
    {
        a += b;
        return a;
    }
    friend String operator+(String a, const char *b)
    {
        a += b;
        return a;
    }
    friend String operator+(const char *a, const String &b)
    {
        String result(a);
        result += b;
        return result;
    }

private:
    std::string value_;
    mutable size_t read_position_ = 0;
};

class HardwareSerial
{
public:
    void begin(unsigned long baud);
    void print(const char *value);
    void print(const String &value);
    void print(char value);
    void print(int value);
    void println();
    void println(const char *value);
    void println(const String &value);
    int printf(const char *format, ...);
};

extern HardwareSerial Serial;

uint32_t millis();
uint32_t micros();
void delay(uint32_t milliseconds);
void delayMicroseconds(uint32_t microseconds);
void yield();
void pinMode(int pin, int mode);
int digitalRead(int pin);
void digitalWrite(int pin, int value);
void analogWrite(int pin, int value);
int analogRead(int pin);
uint32_t touchRead(int pin);
long random(long maximum);
long random(long minimum, long maximum);
long map(
    long value, long from_low, long from_high,
    long to_low, long to_high);
void configTime(
    long gmt_offset_seconds, int daylight_offset_seconds,
    const char *server1, const char *server2 = nullptr,
    const char *server3 = nullptr);
bool getLocalTime(
    struct tm *time_info, uint32_t timeout_ms = 5000);

inline uint8_t lowByte(uint16_t value)
{
    return static_cast<uint8_t>(value);
}

inline uint8_t highByte(uint16_t value)
{
    return static_cast<uint8_t>(value >> 8);
}

size_t strlcpy(char *destination, const char *source, size_t size);

template <typename T>
constexpr const T &min(const T &a, const T &b)
{
    return a < b ? a : b;
}

template <typename T>
constexpr const T &max(const T &a, const T &b)
{
    return a > b ? a : b;
}

template <typename T, typename U, typename V>
constexpr auto constrain(T value, U minimum, V maximum)
{
    return value < minimum
               ? minimum
               : (value > maximum ? maximum : value);
}

#include <esp_heap_caps.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
