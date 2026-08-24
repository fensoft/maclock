#include "control_panel.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include <lvgl.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#if defined(ARDUINO_ARCH_ESP32) && !defined(MACLOCK_LOCAL)
#include <WiFiClient.h>
#include <esp_heap_caps.h>
#include <miniz.h>
#else
#include <zlib.h>
#endif

#include "audio_volume.h"
#include "brightness.h"
#include "configuration_archive.h"
#include "control_panel_page.h"
#include "control_panel_sound_library.h"

LV_FONT_DECLARE(lv_font_chicago_8);
LV_FONT_DECLARE(lv_font_chicago_24);
LV_FONT_DECLARE(lv_font_chicago_32);
LV_FONT_DECLARE(lv_font_chicago_48);
LV_FONT_DECLARE(lv_font_chicago_digits_6);
LV_FONT_DECLARE(lv_font_chicago_digits_10);
LV_FONT_DECLARE(lv_font_chicago_digits_40);
LV_FONT_DECLARE(lv_font_chicago_digits_56);
LV_FONT_DECLARE(lv_font_seven_segment_24);
LV_FONT_DECLARE(lv_font_seven_segment_48);
LV_FONT_DECLARE(lv_font_seven_segment_64);
LV_FONT_DECLARE(lv_font_seven_segment_80);
LV_FONT_DECLARE(lv_font_seven_segment_96);

#include "control_panel_internal.h"


#define g_events (active_control_panel->state().events)
#define g_server (active_control_panel->state().server)
#define g_routes_ready (active_control_panel->state().routes_ready)
#define g_server_running (active_control_panel->state().server_running)
#define g_mdns_running (active_control_panel->state().mdns_running)
#define g_sound_library (active_control_panel->state().sound_library)
#define g_configuration_archive (active_control_panel->state().configuration_archive)
#define g_update_upload_started (active_control_panel->state().update_upload_started)
#define g_update_upload_finished (active_control_panel->state().update_upload_finished)
#define g_update_upload_error (active_control_panel->state().update_upload_error)

ControlPanelService *active_control_panel = nullptr;

void control_panel_queue_download_progress(
    const char *message, uint8_t progress)
{
    auto &state = active_control_panel->state();
    if (state.progress_lock)
        xSemaphoreTake(state.progress_lock, portMAX_DELAY);
    strlcpy(
        state.progress_message, message ? message : "",
        sizeof(state.progress_message));
    state.progress_value = progress;
    state.progress_dirty = true;
    state.progress_hide_pending = false;
    if (state.progress_lock)
        xSemaphoreGive(state.progress_lock);
}

void control_panel_queue_download_hide()
{
    auto &state = active_control_panel->state();
    if (state.progress_lock)
        xSemaphoreTake(state.progress_lock, portMAX_DELAY);
    state.progress_hide_pending = true;
    state.progress_dirty = false;
    if (state.progress_lock)
        xSemaphoreGive(state.progress_lock);
}

void control_panel_send_json(JsonDocument &document, int status)
{
    String response;
    serializeJson(document, response);
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send(status, "application/json", response);
}

void control_panel_send_result(
    bool ok, const char *message, int status)
{
    JsonDocument document;
    document["ok"] = ok;
    document["message"] = message;
    control_panel_send_json(document, status);
}

void send_control_page()
{
    g_server.sendHeader(
        "Cache-Control", "no-store, no-cache, must-revalidate");
    g_server.sendHeader("Content-Encoding", "gzip");
    g_server.send_P(
        200, "text/html",
        reinterpret_cast<PGM_P>(kControlPanelPageGzip),
        kControlPanelPageGzipLength);
}
static void deliver_download_progress(ControlPanelService::State &state)
{
    char message[64] = "";
    uint8_t progress = 0;
    bool dirty = false;
    bool hide = false;
    if (state.progress_lock)
        xSemaphoreTake(state.progress_lock, portMAX_DELAY);
    dirty = state.progress_dirty;
    hide = state.progress_hide_pending;
    if (dirty)
    {
        strlcpy(message, state.progress_message, sizeof(message));
        progress = state.progress_value;
        state.progress_dirty = false;
    }
    if (hide)
        state.progress_hide_pending = false;
    if (state.progress_lock)
        xSemaphoreGive(state.progress_lock);

    if (dirty && state.events)
        state.events->showControlPanelDownload(message, progress);
    if (hide && state.events)
        state.events->hideControlPanelDownload();
}
ControlPanelService::State &ControlPanelService::state()
{
    return *state_;
}

void ControlPanelService::begin(ControlPanelEventSink &events)
{
    if (!state_)
        state_ = new State();
    active_control_panel = this;
    if (!state_->progress_lock)
        state_->progress_lock = xSemaphoreCreateMutex();
    g_events = &events;
    g_configuration_archive.begin(events);
    register_control_panel_routes();
#if defined(MACLOCK_LOCAL)
    start_minivmac_bootstrap(*this);
#endif
}

void ControlPanelService::tick(const WifiModeSnapshot &wifi)
{
    active_control_panel = this;
    deliver_download_progress(*state_);
    if (!wifi.enabled || !wifi.connected || wifi.portal_active)
    {
        stop();
        return;
    }

    if (!g_server_running)
    {
        g_server.begin();
        g_server_running = true;
        g_mdns_running = MDNS.begin("maclock");
        if (g_mdns_running)
            MDNS.addService("http", "tcp", 80);
        Serial.printf(
            "[Control] http://%s/ or http://maclock.local/\n",
            wifi.ip_address);
        start_minivmac_bootstrap(*this);
    }
    g_server.handleClient();
}

void ControlPanelService::stop()
{
    if (!state_)
        return;
    active_control_panel = this;
    if (g_server_running)
    {
        g_server.stop();
        g_server_running = false;
    }
    if (g_mdns_running)
    {
        MDNS.end();
        g_mdns_running = false;
    }
}

bool ControlPanelService::running() const
{
    return state_ && state_->server_running;
}

bool ControlPanelService::backgroundNetworkActive() const
{
    return state_ && state_->minivmac_bootstrap_task;
}
