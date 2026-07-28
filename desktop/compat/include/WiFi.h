#pragma once

#include <Arduino.h>

enum wifi_mode_t
{
    WIFI_MODE_NULL,
    WIFI_OFF = WIFI_MODE_NULL,
    WIFI_STA,
    WIFI_AP
};

enum wl_status_t
{
    WL_IDLE_STATUS,
    WL_NO_SSID_AVAIL,
    WL_SCAN_COMPLETED,
    WL_CONNECTED,
    WL_CONNECT_FAILED,
    WL_CONNECTION_LOST,
    WL_DISCONNECTED
};

#define WIFI_AUTH_OPEN 0
#define WIFI_AUTH_WPA2_PSK 3

class IPAddress
{
public:
    IPAddress(
        uint8_t a = 127, uint8_t b = 0,
        uint8_t c = 0, uint8_t d = 1);
    String toString() const;

private:
    uint8_t bytes_[4];
};

class WiFiClass
{
public:
    wifi_mode_t getMode() const;
    bool mode(wifi_mode_t mode);
    void disconnect(
        bool wifi_off = false, bool erase_ap = false);
    void setAutoReconnect(bool enabled);
    void begin(const char *ssid, const char *password = nullptr);
    wl_status_t status() const;
    int16_t scanNetworks(
        bool asynchronous = false, bool show_hidden = false);
    void scanDelete();
    String SSID(int index) const;
    int32_t RSSI(int index) const;
    int32_t RSSI() const;
    int encryptionType(int index) const;
    IPAddress localIP() const;
    bool softAP(const char *ssid);
    IPAddress softAPIP() const;
    bool softAPdisconnect(bool wifi_off = false);

private:
    wifi_mode_t mode_ = WIFI_MODE_NULL;
    wl_status_t status_ = WL_DISCONNECTED;
    String ssid_;
};

extern WiFiClass WiFi;
