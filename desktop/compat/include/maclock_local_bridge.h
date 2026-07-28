#pragma once

#include <stddef.h>
#include <stdint.h>

class ESP32Encoder;

bool maclock_local_rtc_present();
bool maclock_local_rtc_is_ds1307();
uint32_t maclock_local_rtc_epoch();
void maclock_local_rtc_adjust(uint32_t epoch);
void maclock_local_rtc_reset();

enum class MaclockLocalWeatherSensor : uint8_t
{
    Bmp5xx,
    Htu2x,
    None
};

MaclockLocalWeatherSensor maclock_local_weather_sensor();
uint8_t maclock_local_weather_address();
float maclock_local_temperature();
float maclock_local_pressure();
float maclock_local_humidity();

void maclock_local_register_encoder(ESP32Encoder *encoder);
void maclock_local_audio_rate(uint32_t sample_rate);
