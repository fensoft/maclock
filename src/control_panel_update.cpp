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

namespace
{

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

static const char *update_stage_name(UpdateStage stage)
{
    switch (stage)
    {
    case UpdateStage::Idle:
        return "idle";
    case UpdateStage::Checking:
        return "checking";
    case UpdateStage::UpToDate:
        return "upToDate";
    case UpdateStage::Available:
        return "available";
    case UpdateStage::DownloadingAssets:
        return "downloadingAssets";
    case UpdateStage::InstallingAssets:
        return "installingAssets";
    case UpdateStage::DownloadingFirmware:
        return "downloadingFirmware";
    case UpdateStage::UploadingFirmware:
        return "uploadingFirmware";
    case UpdateStage::ReadyToReboot:
        return "readyToReboot";
    case UpdateStage::Error:
        return "error";
    case UpdateStage::Unsupported:
        return "unsupported";
    }
    return "idle";
}

static void append_update_json_impl(
    JsonObject update, const UpdateSnapshot &snapshot)
{
    update["stage"] = update_stage_name(snapshot.stage);
    update["supported"] = snapshot.supported;
    update["busy"] = snapshot.busy;
    update["available"] = snapshot.update_available;
    update["prompt"] = snapshot.prompt_pending;
    update["rebootRequired"] = snapshot.reboot_required;
    update["progress"] = snapshot.progress;
    update["changedAssets"] = snapshot.changed_assets;
    update["currentVersion"] = snapshot.current_version;
    update["assetVersion"] = snapshot.asset_version;
    update["latestVersion"] = snapshot.latest_version;
    update["releaseUrl"] = snapshot.release_url;
    update["releaseNotes"] = snapshot.release_notes;
    update["message"] = snapshot.message;
}
static void send_update_status()
{
    if (!g_events)
    {
        send_result(false, "Control service is unavailable", 503);
        return;
    }
    const ControlPanelSnapshot snapshot =
        g_events->controlPanelSnapshot();
    JsonDocument document;
    append_update_json_impl(
        document["update"].to<JsonObject>(), snapshot.update);
    send_json(document);
}

static void request_update_check()
{
    const bool requested =
        g_events && g_events->requestControlUpdateCheck();
    send_result(
        requested,
        requested ? "Update check requested"
                  : "Update check could not be started",
        requested ? 202 : 409);
}

static void request_update_install()
{
    const bool requested =
        g_events && g_events->requestControlUpdateInstall();
    send_result(
        requested,
        requested ? "Update installation started"
                  : "Update installation could not be started",
        requested ? 202 : 409);
}

static void dismiss_update()
{
    const String action = g_server.arg("action");
    if (!g_events ||
        (action != "later" && action != "ignore"))
    {
        send_result(false, "Invalid update dismissal", 400);
        return;
    }
    g_events->dismissControlUpdate(action == "ignore");
    send_result(true, "Update notification dismissed");
}

static void receive_firmware_upload()
{
    if (!g_events)
        return;
    HTTPUpload &upload = g_server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        g_update_upload_started =
            g_events->beginControlFirmwareUpload(
                upload.filename.c_str());
        g_update_upload_finished = false;
        g_update_upload_error.clear();
        if (!g_update_upload_started)
            g_update_upload_error =
                "Firmware upload could not be started";
        return;
    }
    if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (g_update_upload_started &&
            !g_events->writeControlFirmwareUpload(
                upload.buf, upload.currentSize))
        {
            g_update_upload_error =
                "Firmware upload failed while writing";
            g_events->abortControlFirmwareUpload();
            g_update_upload_started = false;
        }
        return;
    }
    if (upload.status == UPLOAD_FILE_ABORTED)
    {
        if (g_update_upload_started)
            g_events->abortControlFirmwareUpload();
        g_update_upload_started = false;
        g_update_upload_finished = true;
        g_update_upload_error =
            "Firmware upload was cancelled";
        return;
    }
    if (upload.status == UPLOAD_FILE_END)
    {
        if (g_update_upload_started &&
            !g_events->finishControlFirmwareUpload())
        {
            g_update_upload_error =
                "Firmware validation failed";
        }
        g_update_upload_started = false;
        g_update_upload_finished = true;
    }
}

static void finish_firmware_upload()
{
    if (!g_update_upload_finished)
    {
        send_result(false, "No firmware was uploaded", 400);
        return;
    }
    const bool success = !g_update_upload_error.length();
    send_result(
        success,
        success ? "Firmware uploaded; reboot to finish"
                : g_update_upload_error.c_str(),
        success ? 201 : 400);
}

static void reboot_after_update()
{
    if (!g_events)
    {
        send_result(false, "Control service is unavailable", 503);
        return;
    }
    g_server.send(
        200, "application/json",
        "{\"ok\":true,\"message\":\"Rebooting Maclock\"}");
    delay(100);
    g_events->rebootAfterControlUpdate();
}

void register_control_panel_update_routes_impl(WebServer &server) {
    server.on("/api/update/status", HTTP_GET, send_update_status);
    server.on("/api/update/check", HTTP_POST, request_update_check);
    server.on("/api/update/install", HTTP_POST, request_update_install);
    server.on("/api/update/dismiss", HTTP_POST, dismiss_update);
    server.on("/api/update/reboot", HTTP_POST, reboot_after_update);
    server.on("/api/update/firmware", HTTP_POST, finish_firmware_upload, receive_firmware_upload);
}
} // namespace

void append_update_json(JsonObject update, const UpdateSnapshot &snapshot)
{
    append_update_json_impl(update, snapshot);
}

void register_control_panel_update_routes(WebServer &server)
{
    register_control_panel_update_routes_impl(server);
}
