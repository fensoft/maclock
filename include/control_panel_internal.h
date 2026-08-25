#pragma once

#include "control_panel.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "configuration_archive.h"
#include "control_panel_sound_library.h"

struct ControlPanelService::State
{
    ControlPanelEventSink *events = nullptr;
    WebServer server{80};
    bool routes_ready = false;
    bool server_running = false;
    bool mdns_running = false;
    bool update_upload_started = false;
    bool update_upload_finished = false;
    String update_upload_error;
    ConfigurationArchive configuration_archive;
    ControlPanelSoundLibrary sound_library;
    File minivmac_upload;
    String minivmac_upload_target;
    String minivmac_upload_temp;
    String minivmac_upload_error;
    size_t minivmac_upload_size = 0;
    bool minivmac_upload_started = false;
    bool minivmac_bootstrap_attempted = false;
    TaskHandle_t minivmac_bootstrap_task = nullptr;
    SemaphoreHandle_t progress_lock = nullptr;
    char progress_message[64] = "";
    uint8_t progress_value = 0;
    bool progress_dirty = false;
    bool progress_hide_pending = false;
    File photo_upload;
    String photo_upload_path;
    String photo_upload_error;
    size_t photo_upload_size = 0;
    bool photo_upload_started = false;
    File clockface_upload;
    String clockface_upload_path;
    String clockface_upload_error;
    size_t clockface_upload_size = 0;
    bool clockface_upload_started = false;
    File loading_upload;
    String loading_upload_path;
    String loading_upload_error;
    size_t loading_upload_size = 0;
    bool loading_upload_started = false;
};

extern ControlPanelService *active_control_panel;

void control_panel_send_json(JsonDocument &document, int status = 200);
void control_panel_send_result(bool ok, const char *message, int status = 200);
void control_panel_queue_download_progress(const char *message, uint8_t progress);
void control_panel_queue_download_hide();

inline void send_json(JsonDocument &document, int status = 200)
{
    control_panel_send_json(document, status);
}

inline void send_result(bool ok, const char *message, int status = 200)
{
    control_panel_send_result(ok, message, status);
}

void send_control_page();
void register_control_panel_routes();
void register_control_panel_settings_routes(WebServer &server);
String safe_clockface_name(const String &source);
String safe_loading_name(const String &source);
void register_control_panel_clockface_routes(WebServer &server);
void register_control_panel_loading_routes(WebServer &server);
void register_control_panel_screensaver_routes(WebServer &server);
void register_control_panel_minivmac_routes(WebServer &server);
void register_control_panel_update_routes(WebServer &server);
void start_minivmac_bootstrap(ControlPanelService &service);
void append_update_json(JsonObject update, const UpdateSnapshot &snapshot);
