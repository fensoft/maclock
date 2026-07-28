#include "wifi_mode.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>

#include "localization.h"
#include "maclock_hal.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace
{
static constexpr char kSetupSsid[] = "Maclock Setup";
static constexpr uint32_t kConnectRetryMs = 30000;
static constexpr uint32_t kForecastRefreshMs = 30UL * 60UL * 1000UL;
static constexpr uint32_t kNtpRefreshMs = 6UL * 60UL * 60UL * 1000UL;
static constexpr uint32_t kForecastStaleSeconds = 6UL * 60UL * 60UL;
static constexpr uint16_t kHttpTimeoutMs = 12000;
static constexpr size_t kMaxDetectedNetworks = 12;
static constexpr char kLocalDefaultCity[] = "Paris";
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

} // namespace

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
};

namespace
{
WifiService *active_wifi_service = nullptr;

#define g_preferences (active_wifi_service->state().preferences)
#define g_lock (active_wifi_service->state().lock)
#define g_task (active_wifi_service->state().task)
#define g_settings (active_wifi_service->state().settings)
#define g_snapshot (active_wifi_service->state().snapshot)
#define g_dns_server (active_wifi_service->state().dns_server)
#define g_web_server (active_wifi_service->state().web_server)
#define g_pause_requested (active_wifi_service->state().pause_requested)
#define g_pause_acknowledged \
    (active_wifi_service->state().pause_acknowledged)
#define g_portal_active (active_wifi_service->state().portal_active)
#define g_portal_routes_ready \
    (active_wifi_service->state().portal_routes_ready)
#define g_portal_server_active \
    (active_wifi_service->state().portal_server_active)
#define g_time_sync_pending \
    (active_wifi_service->state().time_sync_pending)
#define g_pending_local_epoch \
    (active_wifi_service->state().pending_local_epoch)
#define g_last_forecast_ms \
    (active_wifi_service->state().last_forecast_ms)
#define g_last_ntp_ms (active_wifi_service->state().last_ntp_ms)
#define g_detected_networks \
    (active_wifi_service->state().detected_networks)
#define g_detected_network_count \
    (active_wifi_service->state().detected_network_count)
#define g_network_scan_succeeded \
    (active_wifi_service->state().network_scan_succeeded)

static void lock_state()
{
    if (g_lock)
        xSemaphoreTake(g_lock, portMAX_DELAY);
}

static void unlock_state()
{
    if (g_lock)
        xSemaphoreGive(g_lock);
}

template <size_t N>
static void copy_text(char (&destination)[N], const String &source)
{
    source.substring(0, N - 1).toCharArray(destination, N);
}

template <size_t N>
static void copy_text(char (&destination)[N], const char *source)
{
    strlcpy(destination, source ? source : "", N);
}

static void set_status(const char *status)
{
    lock_state();
    copy_text(g_snapshot.status, status);
    unlock_state();
}

static WifiSettings settings_snapshot()
{
    lock_state();
    const WifiSettings settings = g_settings;
    unlock_state();
    return settings;
}

static void disconnect_wifi()
{
    if (WiFi.getMode() != WIFI_MODE_NULL)
    {
        WiFi.disconnect(true, false);
        WiFi.mode(WIFI_OFF);
    }
    lock_state();
    g_snapshot.connected = false;
    unlock_state();
}

static void responsive_delay(uint32_t duration_ms)
{
    const uint32_t started = millis();
    while (millis() - started < duration_ms &&
           !g_pause_requested && !g_portal_active)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static String url_encode(const char *text)
{
    static const char hex[] = "0123456789ABCDEF";
    String encoded;
    encoded.reserve(strlen(text) * 2);
    for (const uint8_t c : String(text))
    {
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
        {
            encoded += (char)c;
        }
        else
        {
            encoded += '%';
            encoded += hex[c >> 4];
            encoded += hex[c & 0x0F];
        }
    }
    return encoded;
}

static String html_escape(const char *text)
{
    String escaped;
    escaped.reserve(strlen(text) + 16);
    while (*text)
    {
        switch (*text)
        {
        case '&':
            escaped += F("&amp;");
            break;
        case '<':
            escaped += F("&lt;");
            break;
        case '>':
            escaped += F("&gt;");
            break;
        case '"':
            escaped += F("&quot;");
            break;
        default:
            escaped += *text;
            break;
        }
        ++text;
    }
    return escaped;
}

static void scan_detected_networks()
{
    g_detected_network_count = 0;
    g_network_scan_succeeded = false;
    WiFi.scanDelete();

    const int16_t result =
        WiFi.scanNetworks(false, false);
    if (result < 0)
    {
        Serial.printf("[Wi-Fi] Network scan failed: %d\n", result);
        WiFi.scanDelete();
        return;
    }
    g_network_scan_succeeded = true;

    for (int16_t i = 0; i < result; ++i)
    {
        const String ssid = WiFi.SSID(i);
        if (!ssid.length() || ssid == kSetupSsid)
            continue;

        const int32_t rssi = WiFi.RSSI(i);
        const bool secured =
            WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        size_t destination = g_detected_network_count;
        for (size_t j = 0; j < g_detected_network_count; ++j)
        {
            if (ssid == g_detected_networks[j].ssid)
            {
                destination = j;
                break;
            }
        }

        if (destination < g_detected_network_count)
        {
            if (rssi <= g_detected_networks[destination].rssi)
                continue;
        }
        else if (g_detected_network_count <
                 kMaxDetectedNetworks)
        {
            destination = g_detected_network_count++;
        }
        else
        {
            destination = 0;
            for (size_t j = 1; j < g_detected_network_count; ++j)
            {
                if (g_detected_networks[j].rssi <
                    g_detected_networks[destination].rssi)
                {
                    destination = j;
                }
            }
            if (rssi <= g_detected_networks[destination].rssi)
                continue;
        }

        copy_text(
            g_detected_networks[destination].ssid, ssid);
        g_detected_networks[destination].rssi = rssi;
        g_detected_networks[destination].secured = secured;
    }
    WiFi.scanDelete();

    for (size_t i = 1; i < g_detected_network_count; ++i)
    {
        const DetectedNetwork network =
            g_detected_networks[i];
        size_t j = i;
        while (j > 0 &&
               g_detected_networks[j - 1].rssi <
                   network.rssi)
        {
            g_detected_networks[j] =
                g_detected_networks[j - 1];
            --j;
        }
        g_detected_networks[j] = network;
    }
    Serial.printf(
        "[Wi-Fi] Found %u visible network%s\n",
        (unsigned)g_detected_network_count,
        g_detected_network_count == 1 ? "" : "s");
}

static const char *language_code()
{
    switch (localization_get_language())
    {
    case UI_LANGUAGE_FRENCH:
        return "fr";
    case UI_LANGUAGE_SPANISH:
        return "es";
    case UI_LANGUAGE_GERMAN:
        return "de";
    case UI_LANGUAGE_ITALIAN:
        return "it";
    default:
        return "en";
    }
}

static bool begin_http(HTTPClient &http, NetworkClient &client,
                       const String &url)
{
    http.useHTTP10(true);
    http.setConnectTimeout(kHttpTimeoutMs);
    http.setTimeout(kHttpTimeoutMs);
    return http.begin(client, url);
}

static bool geocode_city(WifiSettings &settings)
{
    set_status("Finding configured city...");
    Serial.printf("[Wi-Fi] Looking up city: %s\n", settings.city);
    const String url =
        String(F("http://geocoding-api.open-meteo.com/v1/search?name=")) +
        url_encode(settings.city) +
        F("&count=1&language=") +
        language_code() +
        F("&format=json");

    NetworkClient client;
    HTTPClient http;
    if (!begin_http(http, client, url))
    {
        set_status("City lookup connection failed");
        Serial.println("[Wi-Fi] Could not start city lookup");
        return false;
    }

    const int response = http.GET();
    if (response != HTTP_CODE_OK)
    {
        set_status(
            response < 0
                ? "City service connection failed"
                : "City service error");
        Serial.printf(
            "[Wi-Fi] City lookup failed: %d (%s)\n",
            response,
            HTTPClient::errorToString(response).c_str());
        http.end();
        return false;
    }
    const String payload = http.getString();
    http.end();
    if (!payload.length())
    {
        set_status("City service returned no data");
        Serial.println("[Wi-Fi] City lookup response was empty");
        return false;
    }

    JsonDocument filter;
    filter["results"][0]["name"] = true;
    filter["results"][0]["country_code"] = true;
    filter["results"][0]["latitude"] = true;
    filter["results"][0]["longitude"] = true;
    filter["results"][0]["timezone"] = true;

    JsonDocument document;
    const DeserializationError error =
        deserializeJson(document, payload,
                        DeserializationOption::Filter(filter));
    if (error)
    {
        set_status("City response could not be read");
        Serial.printf("[Wi-Fi] City JSON error: %s (%u bytes)\n",
                      error.c_str(), (unsigned)payload.length());
        return false;
    }
    if (document["results"][0].isNull())
    {
        set_status("City not found - check spelling");
        Serial.printf("[Wi-Fi] No result for city: %s\n",
                      settings.city);
        return false;
    }

    JsonObject result = document["results"][0];
    settings.latitude = result["latitude"] | 0.0;
    settings.longitude = result["longitude"] | 0.0;
    settings.coordinates_valid = true;

    const char *name = result["name"] | settings.city;
    const char *country = result["country_code"] | "";
    const char *timezone = result["timezone"] | "";
    char location[49];
    if (*country)
        snprintf(location, sizeof(location), "%s, %s", name, country);
    else
        copy_text(location, name);

    lock_state();
    g_settings = settings;
    copy_text(g_snapshot.location, location);
    copy_text(g_snapshot.timezone, timezone);
    unlock_state();

    if (g_preferences)
    {
        g_preferences->putBool("wifi_coord", true);
        g_preferences->putDouble("wifi_lat", settings.latitude);
        g_preferences->putDouble("wifi_lon", settings.longitude);
        g_preferences->putString("wifi_loc", location);
        g_preferences->putString("wifi_tz", timezone);
    }
    Serial.printf(
        "[Wi-Fi] City found: %s (%.5f, %.5f), %s\n",
        location, settings.latitude, settings.longitude, timezone);
    return true;
}

static bool fetch_forecast(WifiSettings &settings)
{
    set_status("Updating online forecast...");
    char url[512];
    snprintf(
        url, sizeof(url),
        "http://api.open-meteo.com/v1/forecast"
        "?latitude=%.6f&longitude=%.6f"
        "&current=temperature_2m,weather_code"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
        "precipitation_probability_max"
        "&forecast_days=1&timezone=auto",
        settings.latitude, settings.longitude);

    NetworkClient client;
    HTTPClient http;
    if (!begin_http(http, client, url))
    {
        Serial.println("[Wi-Fi] Could not start forecast request");
        return false;
    }

    const int response = http.GET();
    if (response != HTTP_CODE_OK)
    {
        Serial.printf("[Wi-Fi] Forecast HTTP status: %d\n", response);
        http.end();
        return false;
    }
    const String payload = http.getString();
    http.end();
    if (!payload.length())
    {
        Serial.println("[Wi-Fi] Forecast response was empty");
        return false;
    }

    JsonDocument filter;
    filter["utc_offset_seconds"] = true;
    filter["timezone"] = true;
    filter["timezone_abbreviation"] = true;
    filter["current"]["temperature_2m"] = true;
    filter["current"]["weather_code"] = true;
    filter["daily"]["temperature_2m_min"][0] = true;
    filter["daily"]["temperature_2m_max"][0] = true;
    filter["daily"]["precipitation_probability_max"][0] = true;

    JsonDocument document;
    const DeserializationError error =
        deserializeJson(document, payload,
                        DeserializationOption::Filter(filter));
    if (error || document["current"].isNull() ||
        document["daily"].isNull())
    {
        Serial.printf("[Wi-Fi] Forecast JSON error: %s (%u bytes)\n",
                      error.c_str(), (unsigned)payload.length());
        return false;
    }

    settings.utc_offset_seconds =
        document["utc_offset_seconds"] | settings.utc_offset_seconds;
    const char *timezone = document["timezone"] | "";

    lock_state();
    g_settings.utc_offset_seconds = settings.utc_offset_seconds;
    g_snapshot.current_temperature =
        document["current"]["temperature_2m"] | 0.0f;
    g_snapshot.weather_code =
        document["current"]["weather_code"] | 0;
    g_snapshot.minimum_temperature =
        document["daily"]["temperature_2m_min"][0] | 0.0f;
    g_snapshot.maximum_temperature =
        document["daily"]["temperature_2m_max"][0] | 0.0f;
    g_snapshot.precipitation_probability =
        document["daily"]["precipitation_probability_max"][0] | 0;
    g_snapshot.forecast_valid = true;
    g_snapshot.forecast_age_seconds = 0;
    if (*timezone)
        copy_text(g_snapshot.timezone, timezone);
    unlock_state();

    if (g_preferences)
    {
        g_preferences->putInt(
            "wifi_offset", settings.utc_offset_seconds);
        if (*timezone)
            g_preferences->putString("wifi_tz", timezone);
    }
    g_last_forecast_ms = millis();
    return true;
}

static bool synchronize_time(const WifiSettings &settings)
{
    set_status("Synchronizing clock...");
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");

    struct tm utc_time = {};
    if (!getLocalTime(&utc_time, 6000))
        return false;

    const time_t utc_epoch = time(nullptr);
    if (utc_epoch < 1704067200)
        return false;

    lock_state();
    g_pending_local_epoch =
        (uint32_t)(utc_epoch + settings.utc_offset_seconds);
    g_time_sync_pending = true;
    unlock_state();
    g_last_ntp_ms = millis();
    return true;
}

static bool connect_station(const WifiSettings &settings)
{
    set_status("Connecting to Wi-Fi...");
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(settings.ssid, settings.password);

    const uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - started < 12000)
    {
        if (g_pause_requested || g_portal_active)
            return false;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    const bool connected = WiFi.status() == WL_CONNECTED;
    lock_state();
    g_snapshot.connected = connected;
    unlock_state();
    return connected;
}

static void wifi_task(void *parameter)
{
    active_wifi_service =
        static_cast<WifiService *>(parameter);
    uint32_t next_connection_attempt = 0;

    for (;;)
    {
        if (g_pause_requested || g_portal_active)
        {
            disconnect_wifi();
            g_pause_acknowledged = true;
            while (g_pause_requested || g_portal_active)
                vTaskDelay(pdMS_TO_TICKS(100));
            g_pause_acknowledged = false;
            next_connection_attempt = 0;
        }

        WifiSettings settings = settings_snapshot();
        if (!settings.enabled)
        {
            disconnect_wifi();
            set_status("Wi-Fi is disabled");
            responsive_delay(500);
            continue;
        }
        if (!settings.ssid[0] || !settings.city[0])
        {
            disconnect_wifi();
            set_status("Setup is required");
            responsive_delay(500);
            continue;
        }

        if (WiFi.status() != WL_CONNECTED)
        {
            const uint32_t now = millis();
            if ((int32_t)(now - next_connection_attempt) < 0)
            {
                vTaskDelay(pdMS_TO_TICKS(250));
                continue;
            }
            if (!connect_station(settings))
            {
                disconnect_wifi();
                set_status("Offline - retrying soon");
                next_connection_attempt = millis() + kConnectRetryMs;
                continue;
            }
            next_connection_attempt = 0;
            g_last_forecast_ms = 0;
            g_last_ntp_ms = 0;
        }

        if (!settings.coordinates_valid &&
            !geocode_city(settings))
        {
            responsive_delay(kConnectRetryMs);
            continue;
        }
        if (g_pause_requested || g_portal_active)
            continue;

        const uint32_t now = millis();
        bool forecast_updated = false;
        if (!g_last_forecast_ms ||
            now - g_last_forecast_ms >= kForecastRefreshMs)
        {
            forecast_updated = fetch_forecast(settings);
            if (!forecast_updated)
                set_status("Forecast unavailable - using sensor");
            settings = settings_snapshot();
        }
        if (g_pause_requested || g_portal_active)
            continue;

        bool time_synchronized = true;
        if (!g_last_ntp_ms ||
            now - g_last_ntp_ms >= kNtpRefreshMs ||
            forecast_updated)
        {
            time_synchronized = synchronize_time(settings);
            if (!time_synchronized)
                set_status("Online - NTP unavailable");
        }

        lock_state();
        g_snapshot.connected = true;
        if (time_synchronized && forecast_updated)
            copy_text(g_snapshot.status, "Online - forecast updated");
        else if (time_synchronized && g_snapshot.forecast_valid)
            copy_text(g_snapshot.status, "Online - cached forecast");
        else if (time_synchronized)
            copy_text(g_snapshot.status, "Online - using local sensor");
        unlock_state();
        responsive_delay(1000);
    }
}

static void send_setup_page()
{
    const WifiSettings settings = settings_snapshot();
    String page;
    page.reserve(5000);
    page += F(
        "<!doctype html><html><head><meta charset=utf-8>"
        "<meta name=viewport "
        "content='width=device-width,initial-scale=1'>"
        "<title>Maclock Wi-Fi</title><style>"
        "body{font:18px sans-serif;max-width:32rem;margin:2rem auto;"
        "padding:0 1rem;background:#eee;color:#111}"
        "form{background:#fff;border:2px solid #111;padding:1rem}"
        "label{display:block;margin:1rem 0 .3rem}"
        "input,button{box-sizing:border-box;width:100%;font:inherit;"
        "padding:.7rem;border:2px solid #111;border-radius:6px}"
        "button{margin-top:1.2rem;background:#111;color:#fff}"
        ".networks{display:grid;gap:.5rem;margin:.5rem 0 1rem}"
        ".network{display:flex;justify-content:space-between;"
        "align-items:center;margin:0;background:#fff;color:#111;"
        "text-align:left}"
        ".network span:first-child{overflow:hidden;text-overflow:ellipsis;"
        "white-space:nowrap}"
        ".network.selected{background:#111;color:#fff}"
        ".network .meta{font-size:.75em;white-space:nowrap;"
        "margin-left:1rem}"
        "small{display:block;margin-top:.4rem}</style></head><body>"
        "<h1>Maclock Wi-Fi</h1><form method=post action=/save>"
        "<label>");
    page += html_escape(tr("Detected networks"));
    page += F("</label><div class=networks>");
    if (g_detected_network_count)
    {
        for (size_t i = 0; i < g_detected_network_count; ++i)
        {
            const DetectedNetwork &network =
                g_detected_networks[i];
            page += F("<button type=button class='network");
            if (strcmp(network.ssid, settings.ssid) == 0)
                page += F(" selected");
            page += F("' data-ssid=\"");
            page += html_escape(network.ssid);
            page += F("\" data-secured=");
            page += network.secured ? F("1") : F("0");
            page += F(" onclick='pickNetwork(this)'><span>");
            page += html_escape(network.ssid);
            page += F("</span><span class=meta>");
            page += network.rssi;
            page += F(" dBm &middot; ");
            page += html_escape(
                tr(network.secured ? "Secured" : "Open network"));
            page += F("</span></button>");
        }
    }
    else
    {
        page += F("<small>");
        page += html_escape(
            tr(g_network_scan_succeeded
                   ? "No networks found. Enter a name below."
                   : "Network scan failed. Enter a name below."));
        page += F("</small>");
    }
    page += F("</div><label>");
    page += html_escape(tr("Wi-Fi name"));
    page += F(
        "</label><input id=ssid name=ssid maxlength=32 required value=\"");
    page += html_escape(settings.ssid);
    page += F(
        "\"><label>");
    page += html_escape(tr("Password"));
    page += F(
        "</label><input id=pass name=pass type=password "
        "maxlength=64 value=\"\"><small>");
    page += html_escape(
        tr("Leave empty to keep the saved password, or for a new open network."));
    page += F("</small><label>");
    page += html_escape(tr("City"));
    page += F(
        "</label><input id=city name=city maxlength=48 required value=\"");
    page += html_escape(settings.city);
    page += F(
        "\"><small>");
    page += html_escape(
        tr("Used for timezone, DST, and weather."));
    page += F("</small><button type=submit>");
    page += html_escape(tr("Save and enable Wi-Fi"));
    page += F(
        "</button>"
        "</form><script>"
        "function pickNetwork(button){"
        "document.getElementById('ssid').value=button.dataset.ssid;"
        "document.querySelectorAll('.network').forEach("
        "item=>item.classList.remove('selected'));"
        "button.classList.add('selected');"
        "document.getElementById("
        "button.dataset.secured==='1'?'pass':'city').focus();"
        "}"
        "</script></body></html>");
    g_web_server.send(200, "text/html", page);
}

static void save_setup()
{
    String ssid = g_web_server.arg("ssid");
    String password = g_web_server.arg("pass");
    String city = g_web_server.arg("city");
    ssid.trim();
    city.trim();
    if (!ssid.length() || !city.length())
    {
        g_web_server.send(400, "text/plain",
                          tr("Wi-Fi name and city are required."));
        return;
    }

    lock_state();
    const bool keep_password =
        !password.length() && ssid == g_settings.ssid;
    char saved_password[65];
    copy_text(saved_password, g_settings.password);
    g_settings.enabled = true;
    g_settings.coordinates_valid = false;
    copy_text(g_settings.ssid, ssid);
    if (keep_password)
        copy_text(g_settings.password, saved_password);
    else
        copy_text(g_settings.password, password);
    copy_text(g_settings.city, city);
    g_snapshot.enabled = true;
    g_snapshot.configured = true;
    copy_text(g_snapshot.ssid, ssid);
    copy_text(g_snapshot.city, city);
    copy_text(g_snapshot.location, "");
    copy_text(g_snapshot.timezone, "");
    copy_text(g_snapshot.status, "Saved - exit setup to connect");
    g_snapshot.forecast_valid = false;
    unlock_state();

    if (g_preferences)
    {
        g_preferences->putBool("wifi_on", true);
        g_preferences->putString("wifi_ssid", ssid);
        if (!keep_password)
            g_preferences->putString("wifi_pass", password);
        g_preferences->putString("wifi_city", city);
        g_preferences->putBool("wifi_coord", false);
    }

    String page;
    page.reserve(256);
    page += F(
        "<!doctype html><meta charset=utf-8>"
        "<meta name=viewport "
        "content='width=device-width,initial-scale=1'>"
        "<h1>");
    page += html_escape(tr("Saved"));
    page += F("</h1><p>");
    page += html_escape(
        tr("Return to Maclock and press Back."));
    page += F("</p>");
    g_web_server.send(200, "text/html", page);
}

static void configure_portal_routes()
{
    if (g_portal_routes_ready)
        return;
    g_web_server.on("/", HTTP_GET, send_setup_page);
    g_web_server.on("/save", HTTP_POST, save_setup);
    g_web_server.on("/generate_204", HTTP_ANY, send_setup_page);
    g_web_server.on("/gen_204", HTTP_ANY, send_setup_page);
    g_web_server.on("/hotspot-detect.html", HTTP_ANY, send_setup_page);
    g_web_server.on(
        "/library/test/success.html",
        HTTP_ANY, send_setup_page);
    g_web_server.onNotFound(send_setup_page);
    g_portal_routes_ready = true;
}

static bool has_saved_wifi_settings(
    Preferences &preferences)
{
    static constexpr const char *keys[] = {
        "wifi_on", "wifi_ssid", "wifi_pass", "wifi_city",
        "wifi_coord", "wifi_lat", "wifi_lon", "wifi_loc",
        "wifi_tz", "wifi_offset"};
    for (const char *key : keys)
    {
        if (preferences.isKey(key))
            return true;
    }
    return false;
}

static int32_t local_utc_offset_seconds()
{
    const time_t now = time(nullptr);
    struct tm local_time = {};
    struct tm utc_time = {};
    if (now == static_cast<time_t>(-1) ||
        !localtime_r(&now, &local_time) ||
        !gmtime_r(&now, &utc_time))
    {
        return 0;
    }

    // Ask mktime to interpret both values using the host's current
    // daylight-saving state, then compare the resulting epochs.
    utc_time.tm_isdst = local_time.tm_isdst;
    const time_t local_epoch = mktime(&local_time);
    const time_t utc_as_local_epoch = mktime(&utc_time);
    if (local_epoch == static_cast<time_t>(-1) ||
        utc_as_local_epoch == static_cast<time_t>(-1))
    {
        return 0;
    }
    return static_cast<int32_t>(
        difftime(local_epoch, utc_as_local_epoch));
}

static void seed_local_wifi_defaults(
    Preferences &preferences)
{
    if (!maclock_hal_installed() ||
        !maclock_hal().isLocal() ||
        has_saved_wifi_settings(preferences))
    {
        return;
    }

    preferences.putBool("wifi_on", true);
    preferences.putString(
        "wifi_ssid",
        maclock_hal().network().simulatedSsid());
    preferences.putString("wifi_pass", "");
    preferences.putString("wifi_city", kLocalDefaultCity);
    preferences.putBool("wifi_coord", true);
    preferences.putDouble(
        "wifi_lat", kLocalDefaultLatitude);
    preferences.putDouble(
        "wifi_lon", kLocalDefaultLongitude);
    preferences.putString(
        "wifi_loc", kLocalDefaultLocation);
    preferences.putString(
        "wifi_tz", kLocalDefaultTimezone);
    preferences.putInt(
        "wifi_offset", local_utc_offset_seconds());
    Serial.println(
        "[Wi-Fi] Seeded fresh simulator Wi-Fi defaults");
}
} // namespace

WifiService::State &WifiService::state()
{
    return *state_;
}

void WifiService::begin(Preferences &preferences)
{
    if (!state_)
        state_ = new State();
    active_wifi_service = this;
    g_preferences = &preferences;
    if (!g_lock)
        g_lock = xSemaphoreCreateMutex();

    seed_local_wifi_defaults(preferences);

    WifiSettings settings = {};
    settings.enabled = preferences.getBool("wifi_on", false);
    settings.coordinates_valid =
        preferences.getBool("wifi_coord", false);
    copy_text(settings.ssid,
              preferences.getString("wifi_ssid", ""));
    copy_text(settings.password,
              preferences.getString("wifi_pass", ""));
    copy_text(settings.city,
              preferences.getString("wifi_city", ""));
    settings.latitude = preferences.isKey("wifi_lat")
                            ? preferences.getDouble("wifi_lat", 0.0)
                            : 0.0;
    settings.longitude = preferences.isKey("wifi_lon")
                             ? preferences.getDouble("wifi_lon", 0.0)
                             : 0.0;
    settings.utc_offset_seconds =
        preferences.getInt("wifi_offset", 0);

    lock_state();
    g_settings = settings;
    g_snapshot.enabled = settings.enabled;
    g_snapshot.configured =
        settings.ssid[0] != '\0' && settings.city[0] != '\0';
    copy_text(g_snapshot.ssid, settings.ssid);
    copy_text(g_snapshot.city, settings.city);
    copy_text(
        g_snapshot.location,
        preferences.isKey("wifi_loc")
            ? preferences.getString("wifi_loc", "")
            : String());
    copy_text(
        g_snapshot.timezone,
        preferences.isKey("wifi_tz")
            ? preferences.getString("wifi_tz", "")
            : String());
    copy_text(
        g_snapshot.status,
        settings.enabled
            ? (g_snapshot.configured
                   ? "Waiting to connect"
                   : "Setup is required")
            : "Wi-Fi is disabled");
    unlock_state();
}

void WifiService::startTask()
{
    if (g_task)
        return;
    const BaseType_t created = xTaskCreatePinnedToCore(
        wifi_task, "wifi_task", 10240, this, 1, &g_task, 0);
    if (created != pdPASS)
    {
        g_task = nullptr;
        set_status("Wi-Fi worker could not start");
    }
}

void WifiService::setEnabled(bool enabled)
{
    lock_state();
    g_settings.enabled = enabled;
    g_snapshot.enabled = enabled;
    copy_text(g_snapshot.status,
              enabled
                  ? (g_snapshot.configured
                         ? "Waiting to connect"
                         : "Setup is required")
                  : "Wi-Fi is disabled");
    unlock_state();
    if (g_preferences)
        g_preferences->putBool("wifi_on", enabled);
}

WifiModeSnapshot WifiService::snapshot()
{
    lock_state();
    WifiModeSnapshot snapshot = g_snapshot;
    if (!snapshot.enabled || !snapshot.connected)
    {
        snapshot.forecast_valid = false;
    }
    else if (snapshot.forecast_valid)
    {
        snapshot.forecast_age_seconds =
            (millis() - g_last_forecast_ms) / 1000;
        if (snapshot.forecast_age_seconds >
            kForecastStaleSeconds)
        {
            snapshot.forecast_valid = false;
        }
    }
    unlock_state();

    if (snapshot.connected && WiFi.status() == WL_CONNECTED)
    {
        copy_text(snapshot.ip_address, WiFi.localIP().toString());
        snapshot.rssi = WiFi.RSSI();
    }
    else
    {
        copy_text(snapshot.ip_address, "");
        snapshot.rssi = 0;
    }
    return snapshot;
}

void WifiService::startPortal()
{
    if (g_portal_active)
        return;

    pause();
    configure_portal_routes();
    WiFi.mode(WIFI_STA);
    delay(50);
    scan_detected_networks();
    WiFi.mode(WIFI_AP);
    const bool access_point_started = WiFi.softAP(kSetupSsid);
    delay(100);
    if (access_point_started)
    {
        g_dns_server.start(53, "*", WiFi.softAPIP());
        g_web_server.begin();
        g_portal_server_active = true;
    }
    g_portal_active = true;

    lock_state();
    g_snapshot.portal_active = true;
    g_snapshot.connected = false;
    copy_text(g_snapshot.status,
              access_point_started
                  ? "Connect to Maclock Setup"
                  : "Could not start setup network");
    unlock_state();
}

void WifiService::processPortal()
{
    if (!g_portal_active)
        return;
    if (!g_portal_server_active)
        return;
    g_dns_server.processNextRequest();
    g_web_server.handleClient();
}

void WifiService::stopPortal()
{
    if (!g_portal_active)
        return;
    if (g_portal_server_active)
    {
        g_dns_server.stop();
        g_web_server.stop();
        g_portal_server_active = false;
    }
    WiFi.softAPdisconnect(true);
    g_portal_active = false;

    lock_state();
    g_snapshot.portal_active = false;
    unlock_state();
    resume();
}

bool WifiService::takeTimeSync(uint32_t &local_epoch)
{
    lock_state();
    const bool pending = g_time_sync_pending;
    if (pending)
    {
        local_epoch = g_pending_local_epoch;
        g_time_sync_pending = false;
    }
    unlock_state();
    return pending;
}

void WifiService::pause()
{
    if (!g_task)
        return;
    g_pause_requested = true;
    const uint32_t started = millis();
    while (!g_pause_acknowledged &&
           millis() - started < 15000)
    {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void WifiService::resume()
{
    g_pause_requested = false;
}
