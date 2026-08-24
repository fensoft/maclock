#pragma once

#include "wifi_mode.h"

#include <DNSServer.h>
#include <WebServer.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static constexpr char kSetupSsid[] = "Maclock Setup";
static constexpr uint32_t kConnectRetryMs = 30000;
static constexpr uint32_t kForecastRefreshMs = 30UL * 60UL * 1000UL;
static constexpr uint32_t kNtpRefreshMs = 6UL * 60UL * 60UL * 1000UL;
static constexpr uint32_t kForecastStaleSeconds = 6UL * 60UL * 60UL;
static constexpr uint16_t kHttpTimeoutMs = 12000;
static constexpr size_t kMaxDetectedNetworks = 12;
static constexpr char kLocalDefaultCity[] = "Paris";
static constexpr char kLocalDefaultCountry[] = "FR";
static constexpr char kLocalDefaultLocation[] = "Paris, FR";
static constexpr char kLocalDefaultTimezone[] = "Europe/Paris";
static constexpr double kLocalDefaultLatitude = 48.856613;
static constexpr double kLocalDefaultLongitude = 2.352222;

struct WifiSettings
{
    bool enabled;
    bool coordinates_valid;
    char ssid[33];
    char password[65];
    char city[49];
    char country[3];
    double latitude;
    double longitude;
    int32_t utc_offset_seconds;
};

struct DetectedNetwork
{
    char ssid[33];
    int32_t rssi;
    bool secured;
};

struct WifiService::State
{
    Preferences *preferences = nullptr;
    SemaphoreHandle_t lock = nullptr;
    TaskHandle_t task = nullptr;
    WifiSettings settings = {};
    WifiModeSnapshot snapshot = {};
    DNSServer dns_server;
    WebServer web_server{80};
    volatile bool pause_requested = false;
    volatile bool pause_acknowledged = false;
    volatile bool portal_active = false;
    bool portal_routes_ready = false;
    bool portal_server_active = false;
    bool time_sync_pending = false;
    uint32_t pending_local_epoch = 0;
    uint32_t last_forecast_ms = 0;
    uint32_t last_ntp_ms = 0;
    DetectedNetwork detected_networks[kMaxDetectedNetworks] = {};
    size_t detected_network_count = 0;
    bool network_scan_succeeded = false;
    WifiBackupSettings pending_restore = {};
    volatile bool restore_pending = false;
    uint32_t restore_due_ms = 0;
};

extern WifiService *active_wifi_service;

inline WifiService::State &wifi_state()
{
    return active_wifi_service->state();
}

inline void lock_state()
{
    if (wifi_state().lock)
        xSemaphoreTake(wifi_state().lock, portMAX_DELAY);
}

inline void unlock_state()
{
    if (wifi_state().lock)
        xSemaphoreGive(wifi_state().lock);
}

template <size_t N>
inline void copy_text(char (&destination)[N], const String &source)
{
    source.substring(0, N - 1).toCharArray(destination, N);
}

template <size_t N>
inline void copy_text(char (&destination)[N], const char *source)
{
    strlcpy(destination, source ? source : "", N);
}

bool normalize_country_code(String &country);
void set_status(const char *status);
WifiSettings settings_snapshot();
void apply_backup_settings(const WifiBackupSettings &backup);
void disconnect_wifi();
void cache_station_details(bool connected);
void responsive_delay(uint32_t duration_ms);

bool geocode_city(WifiSettings &settings);
bool fetch_forecast(WifiSettings &settings);
bool synchronize_time(const WifiSettings &settings);

void wifi_settings_begin(Preferences &preferences);
void wifi_station_start_task(WifiService &service);
void wifi_portal_start();
void wifi_portal_process();
void wifi_portal_stop();

#define g_preferences (wifi_state().preferences)
#define g_lock (wifi_state().lock)
#define g_task (wifi_state().task)
#define g_settings (wifi_state().settings)
#define g_snapshot (wifi_state().snapshot)
#define g_dns_server (wifi_state().dns_server)
#define g_web_server (wifi_state().web_server)
#define g_pause_requested (wifi_state().pause_requested)
#define g_pause_acknowledged (wifi_state().pause_acknowledged)
#define g_portal_active (wifi_state().portal_active)
#define g_portal_routes_ready (wifi_state().portal_routes_ready)
#define g_portal_server_active (wifi_state().portal_server_active)
#define g_time_sync_pending (wifi_state().time_sync_pending)
#define g_pending_local_epoch (wifi_state().pending_local_epoch)
#define g_last_forecast_ms (wifi_state().last_forecast_ms)
#define g_last_ntp_ms (wifi_state().last_ntp_ms)
#define g_detected_networks (wifi_state().detected_networks)
#define g_detected_network_count (wifi_state().detected_network_count)
#define g_network_scan_succeeded (wifi_state().network_scan_succeeded)
#define g_pending_restore (wifi_state().pending_restore)
#define g_restore_pending (wifi_state().restore_pending)
#define g_restore_due_ms (wifi_state().restore_due_ms)
