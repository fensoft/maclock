#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct WifiModeSnapshot
{
    bool enabled;
    bool configured;
    bool connected;
    bool portal_active;
    bool forecast_valid;
    char ssid[33];
    char city[49];
    char location[49];
    char timezone[41];
    char status[65];
    char ip_address[16];
    int32_t rssi;
    float current_temperature;
    float minimum_temperature;
    float maximum_temperature;
    uint8_t precipitation_probability;
    uint8_t weather_code;
    uint32_t forecast_age_seconds;
};

void wifi_mode_begin(Preferences &preferences);
void wifi_mode_start_task();
void wifi_mode_set_enabled(bool enabled);
WifiModeSnapshot wifi_mode_snapshot();

void wifi_mode_start_portal();
void wifi_mode_process_portal();
void wifi_mode_stop_portal();

bool wifi_mode_take_time_sync(uint32_t &local_epoch);
void wifi_mode_pause();
void wifi_mode_resume();
