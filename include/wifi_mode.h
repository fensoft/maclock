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
    char country[3];
    float current_temperature;
    float minimum_temperature;
    float maximum_temperature;
    uint8_t precipitation_probability;
    uint8_t weather_code;
    uint32_t forecast_age_seconds;
};

struct WifiBackupSettings
{
    bool enabled = false;
    bool coordinates_valid = false;
    char ssid[33] = "";
    char city[49] = "";
    char country[3] = "";
    char location[49] = "";
    char timezone[41] = "";
    double latitude = 0.0;
    double longitude = 0.0;
    int32_t utc_offset_seconds = 0;
};

class WifiService
{
public:
    struct State;

    void begin(Preferences &preferences);
    void startTask();
    void setEnabled(bool enabled);
    bool setLocation(const char *city, const char *country);
    WifiModeSnapshot snapshot();
    WifiBackupSettings backupSettings();
    bool restoreSettings(
        const WifiBackupSettings &settings,
        bool &network_changed);

    void startPortal();
    void processPortal();
    void stopPortal();

    bool takeTimeSync(uint32_t &local_epoch);
    void pause();
    void resume();

    State &state();

private:
    State *state_ = nullptr;
};
