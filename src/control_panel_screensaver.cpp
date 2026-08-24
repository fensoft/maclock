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

static bool is_jpeg_name(const String &name)
{
    const size_t length = name.length();
    return (length >= 4 && strcasecmp(name.c_str() + length - 4, ".jpg") == 0) ||
           (length >= 5 && strcasecmp(name.c_str() + length - 5, ".jpeg") == 0);
}

static String safe_photo_name(const String &source)
{
    String name;
    for (size_t i = 0; i < source.length() && name.length() < 72; ++i)
    {
        const char c = source[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_') name += c;
        else if (c == '.' || c == ' ') name += c == ' ' ? '-' : c;
    }
    return is_jpeg_name(name) ? name : String();
}

static void send_screensaver_photos()
{
    JsonDocument document;
    JsonArray photos = document["photos"].to<JsonArray>();
    File directory = LittleFS.open("/screensaver");
    if (directory && directory.isDirectory())
    {
        for (File file = directory.openNextFile(); file;
             file = directory.openNextFile())
        {
            const String path = file.name();
            if (file.isDirectory() || !is_jpeg_name(path))
                continue;
            const int slash = path.lastIndexOf("/");
            JsonObject photo = photos.add<JsonObject>();
            photo["name"] = slash >= 0 ? path.substring(slash + 1) : path;
            photo["size"] = file.size();
        }
        directory.close();
    }
    send_json(document);
}

static bool requested_photo_path(String &path)
{
    if (!g_server.hasArg("name"))
        return false;
    const String name = safe_photo_name(g_server.arg("name"));
    if (!name.length())
        return false;
    path = String("/screensaver/") + name;
    return true;
}

static void send_screensaver_photo()
{
    String path;
    if (!requested_photo_path(path) || !LittleFS.exists(path.c_str()))
    {
        send_result(false, "Photo not found", 404);
        return;
    }
    File file = LittleFS.open(path.c_str(), "r");
    if (!file)
    {
        send_result(false, "Photo could not be opened", 500);
        return;
    }
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.setContentLength(file.size());
    g_server.send(200, "image/jpeg", "");
    uint8_t buffer[1024];
    while (file.position() < file.size())
    {
        const size_t count = file.read(buffer, sizeof(buffer));
        if (!count)
            break;
        g_server.sendContent(
            reinterpret_cast<const char *>(buffer), count);
    }
    file.close();
}

static void receive_screensaver_photo_upload()
{
    HTTPUpload &upload = g_server.upload();
    auto &state = active_control_panel->state();
    if (upload.status == UPLOAD_FILE_START)
    {
        state.photo_upload_error = "";
        state.photo_upload_size = 0;
        state.photo_upload_started = true;
        const String name = safe_photo_name(upload.filename);
        if (!name.length())
        {
            state.photo_upload_error = "Only JPEG photos are accepted";
            return;
        }
        if (!LittleFS.exists("/screensaver") &&
            !LittleFS.mkdir("/screensaver"))
        {
            state.photo_upload_error = "Could not create photo folder";
            return;
        }
        state.photo_upload_path = String("/screensaver/") + name;
        state.photo_upload = LittleFS.open(
            state.photo_upload_path.c_str(), "w");
        if (!state.photo_upload)
            state.photo_upload_error = "Could not create photo file";
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (!state.photo_upload_error.length() &&
            state.photo_upload.write(upload.buf, upload.currentSize) !=
                upload.currentSize)
            state.photo_upload_error = "Could not write photo file";
        state.photo_upload_size += upload.currentSize;
        if (state.photo_upload_size > 1024U * 1024U)
            state.photo_upload_error = "Photo file is too large";
    }
    else if (upload.status == UPLOAD_FILE_END ||
             upload.status == UPLOAD_FILE_ABORTED)
    {
        if (state.photo_upload)
            state.photo_upload.close();
        if (upload.status == UPLOAD_FILE_ABORTED)
            state.photo_upload_error = "Photo upload was cancelled";
        if (state.photo_upload_error.length())
            LittleFS.remove(state.photo_upload_path.c_str());
    }
}

static void finish_screensaver_photo_upload()
{
    auto &state = active_control_panel->state();
    if (!state.photo_upload_started)
    {
        send_result(false, "No photo was uploaded", 400);
        return;
    }
    state.photo_upload_started = false;
    if (state.photo_upload_error.length())
    {
        send_result(false, state.photo_upload_error.c_str(), 400);
        return;
    }
    send_result(true, "Photo uploaded");
}

static void delete_screensaver_photo()
{
    String path;
    if (!requested_photo_path(path) || !LittleFS.exists(path.c_str()))
    {
        send_result(false, "Photo not found", 404);
        return;
    }
    send_result(
        LittleFS.remove(path.c_str()),
        "Photo deleted");
}

void register_control_panel_screensaver_routes_impl(WebServer &server) {
    server.on("/api/screensaver/photos", HTTP_GET, send_screensaver_photos);
    server.on("/api/screensaver/photo", HTTP_GET, send_screensaver_photo);
    server.on("/api/screensaver/photo/upload", HTTP_POST, finish_screensaver_photo_upload, receive_screensaver_photo_upload);
    server.on("/api/screensaver/photo/delete", HTTP_POST, delete_screensaver_photo);
}
} // namespace

void register_control_panel_screensaver_routes(WebServer &server)
{
    register_control_panel_screensaver_routes_impl(server);
}
