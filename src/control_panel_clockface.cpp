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
    return (length >= 4 &&
            strcasecmp(name.c_str() + length - 4, ".jpg") == 0) ||
           (length >= 5 &&
            strcasecmp(name.c_str() + length - 5, ".jpeg") == 0);
}

static String safe_photo_name(const String &source)
{
    String name;
    for (size_t i = 0; i < source.length() && name.length() < 72; ++i)
    {
        const char c = source[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_')
            name += c;
        else if (c == '.' || c == ' ')
            name += c == ' ' ? '-' : c;
    }
    if (!is_jpeg_name(name))
        return String();
    return name;
}

String safe_clockface_name_impl(const String &source)
{
    String name;
    for (size_t i = 0; i < source.length() && name.length() < 48; ++i)
    {
        const char c = source[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_')
            name += c;
    }
    return name;
}

static String safe_clockface_asset_name(const String &source)
{
    String name;
    for (size_t i = 0; i < source.length() && name.length() < 64; ++i)
    {
        const char c = source[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.')
            name += c;
    }
    const size_t length = name.length();
    return length >= 4 &&
            strcasecmp(name.c_str() + length - 4, ".png") == 0
        ? name : String();
}

static bool clockface_has_suffix(
    const String &value, const char *suffix)
{
    const size_t value_length = value.length();
    const size_t suffix_length = suffix ? strlen(suffix) : 0;
    return suffix_length <= value_length &&
        strcmp(value.c_str() + value_length - suffix_length, suffix) == 0;
}

static void send_clockface_file(File &file, const char *mime)
{
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.setContentLength(file.size());
    g_server.send(200, mime, "");
    uint8_t buffer[1024];
    while (file.position() < file.size())
    {
        const size_t count = file.read(buffer, sizeof(buffer));
        if (!count)
            break;
        g_server.sendContent(
            reinterpret_cast<const char *>(buffer), count);
    }
}

static bool requested_clockface_name(String &name)
{
    if (!g_server.hasArg("face") && !g_server.hasArg("name"))
        return false;
    name = safe_clockface_name_impl(
        g_server.hasArg("face") ? g_server.arg("face") :
                                  g_server.arg("name"));
    return name.length() > 0;
}

static void send_clockface_list()
{
    LittleFS.mkdir("/clockface");
    JsonDocument document;
    JsonArray faces = document["faces"].to<JsonArray>();
    File directory = LittleFS.open("/clockface");
    if (directory && directory.isDirectory())
    {
        for (File file = directory.openNextFile(); file;
             file = directory.openNextFile())
        {
            const String path = file.name();
            if (file.isDirectory())
            {
                const int slash = path.lastIndexOf("/");
                const String name = safe_clockface_name_impl(
                    path.substring(slash + 1));
                File project = name.length() ? LittleFS.open(
                    (String("/clockface/") + name + "/clockface.json").c_str(),
                    "r") : File();
                if (project)
                {
                    faces.add(name);
                    project.close();
                }
            }
        }
        directory.close();
    }
    send_json(document);
}

static void send_clockface_fonts()
{
    struct FontInfo
    {
        const char *id;
        uint8_t size;
        bool digits_only;
    };
    static constexpr FontInfo fonts[] = {
        {"lv_font_chicago_8", 8, false},
        {"lv_font_chicago_24", 24, false},
        {"lv_font_chicago_32", 32, false},
        {"lv_font_chicago_48", 48, false},
        {"lv_font_chicago_digits_6", 6, true},
        {"lv_font_chicago_digits_10", 10, true},
        {"lv_font_chicago_digits_40", 40, true},
        {"lv_font_chicago_digits_56", 56, true},
        {"lv_font_seven_segment_24", 24, false},
        {"lv_font_seven_segment_48", 48, false},
        {"lv_font_seven_segment_64", 64, false},
        {"lv_font_seven_segment_80", 80, false},
        {"lv_font_seven_segment_96", 96, false},
    };
    JsonDocument document;
    JsonArray result = document["fonts"].to<JsonArray>();
    for (const FontInfo &font : fonts)
    {
        JsonObject entry = result.add<JsonObject>();
        entry["id"] = font.id;
        entry["size"] = font.size;
        entry["digitsOnly"] = font.digits_only;
        entry["pixelPerfect"] = true;
    }
    send_json(document);
}

static const lv_font_t *clockface_font(const String &id)
{
    if (id == "lv_font_chicago_8") return &lv_font_chicago_8;
    if (id == "lv_font_chicago_24") return &lv_font_chicago_24;
    if (id == "lv_font_chicago_32") return &lv_font_chicago_32;
    if (id == "lv_font_chicago_48") return &lv_font_chicago_48;
    if (id == "lv_font_chicago_digits_6") return &lv_font_chicago_digits_6;
    if (id == "lv_font_chicago_digits_10") return &lv_font_chicago_digits_10;
    if (id == "lv_font_chicago_digits_40") return &lv_font_chicago_digits_40;
    if (id == "lv_font_chicago_digits_56") return &lv_font_chicago_digits_56;
    if (id == "lv_font_seven_segment_24") return &lv_font_seven_segment_24;
    if (id == "lv_font_seven_segment_48") return &lv_font_seven_segment_48;
    if (id == "lv_font_seven_segment_64") return &lv_font_seven_segment_64;
    if (id == "lv_font_seven_segment_80") return &lv_font_seven_segment_80;
    if (id == "lv_font_seven_segment_96") return &lv_font_seven_segment_96;
    return nullptr;
}

static void send_clockface_glyph()
{
    const String spec = g_server.arg("spec");
    const int separator = spec.lastIndexOf("__");
    const String font_id = separator > 0 ?
        spec.substring(0, separator) : String();
    const lv_font_t *font = clockface_font(font_id);
    const uint32_t code = static_cast<uint32_t>(
        strtoul(separator > 0 ? spec.substring(separator + 2).c_str() :
                              "0", nullptr, 10));
    if (!font || code < 0x20 || code > 0xFF)
    {
        send_result(false, "Invalid clock-face glyph", 400);
        return;
    }
    lv_font_glyph_dsc_t glyph{};
    if (!lv_font_get_glyph_dsc(font, &glyph, code, 0))
    {
        send_result(false, "Glyph is unavailable", 404);
        return;
    }
    glyph.req_raw_bitmap = 1;
    const uint8_t *bitmap = static_cast<const uint8_t *>(
        font->get_glyph_bitmap(&glyph, nullptr));
    if (!bitmap && glyph.box_w && glyph.box_h)
    {
        lv_font_glyph_release_draw_data(&glyph);
        send_result(false, "Glyph bitmap is unavailable", 404);
        return;
    }
    String pixels;
    pixels.reserve(glyph.box_w * glyph.box_h);
    uint32_t bit = 0;
    for (uint16_t y = 0; y < glyph.box_h; ++y)
        for (uint16_t x = 0; x < glyph.box_w; ++x, ++bit)
            pixels += bitmap[bit / 8] & (0x80U >> (bit & 7)) ? '1' : '0';
    lv_font_glyph_release_draw_data(&glyph);
    JsonDocument document;
    document["advance"] = glyph.adv_w;
    document["width"] = glyph.box_w;
    document["height"] = glyph.box_h;
    document["offsetX"] = glyph.ofs_x;
    document["offsetY"] = glyph.ofs_y;
    document["lineHeight"] = font->line_height;
    document["baseLine"] = font->base_line;
    document["pixels"] = pixels;
    send_json(document);
}

static void send_clockface_project()
{
    String name;
    const String path = requested_clockface_name(name)
        ? String("/clockface/") + name + "/clockface.json" : String();
    File file = path.length() ? LittleFS.open(path.c_str(), "r") : File();
    if (!file)
    {
        send_result(false, "Clock face not found", 404);
        return;
    }
    send_clockface_file(file, "application/json");
    file.close();
}

static void send_clockface_assets()
{
    String name;
    if (!requested_clockface_name(name))
    {
        send_result(false, "Clock face not found", 404);
        return;
    }
    JsonDocument document;
    JsonArray assets = document["assets"].to<JsonArray>();
    File directory = LittleFS.open((String("/clockface/") + name).c_str());
    if (!directory || !directory.isDirectory())
    {
        send_result(false, "Clock face not found", 404);
        return;
    }
    for (File file = directory.openNextFile(); file;
         file = directory.openNextFile())
    {
        if (file.isDirectory())
            continue;
        const String path = file.name();
        const int slash = path.lastIndexOf("/");
        const String asset = safe_clockface_asset_name(
            slash >= 0 ? path.substring(slash + 1) : path);
        if (asset.length())
            assets.add(asset);
    }
    directory.close();
    send_json(document);
}

static void save_clockface_project()
{
    String name;
    const String json = g_server.arg("json");
    if (!requested_clockface_name(name) || !json.length() ||
        json.length() > 32768)
    {
        send_result(false, "Invalid clock face project", 400);
        return;
    }
    JsonDocument document;
    if (deserializeJson(document, json) ||
        document["format"] != "maclock-clock-face" ||
        document["width"] != 304 || document["height"] != 224)
    {
        send_result(false, "Invalid clock face JSON", 400);
        return;
    }
    LittleFS.mkdir("/clockface");
    LittleFS.mkdir((String("/clockface/") + name).c_str());
    File file = LittleFS.open(
        (String("/clockface/") + name + "/clockface.json").c_str(), "w");
    if (!file || file.write(
            reinterpret_cast<const uint8_t *>(json.c_str()),
            json.length()) != json.length())
    {
        if (file)
            file.close();
        send_result(false, "Could not save clock face", 500);
        return;
    }
    file.close();
    send_result(true, "Clock face saved");
}

static void select_clockface_project()
{
    if (!g_events)
    {
        send_result(false, "Control service is unavailable", 503);
        return;
    }
    String name;
    const bool clear = g_server.arg("clear") == "1";
    if (!clear && !requested_clockface_name(name))
    {
        send_result(false, "Invalid clock face name", 400);
        return;
    }
    if (!clear && !LittleFS.exists(
            (String("/clockface/") + name + "/clockface.json").c_str()))
    {
        send_result(false, "Clock face not found", 404);
        return;
    }
    const ControlPanelSnapshot snapshot = g_events->controlPanelSnapshot();
    const bool applied = g_events->applyControlAppearance(
        snapshot.settings.language, snapshot.brightness,
        snapshot.settings.face_customization, snapshot.settings.time_format,
        clear ? "" : name.c_str(), snapshot.settings.loading_screen);
    send_result(applied, applied ? "Clock face selected" : "Clock face was not selected", applied ? 200 : 500);
}

static void receive_clockface_asset_upload()
{
    auto &state = active_control_panel->state();
    HTTPUpload &upload = g_server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        state.clockface_upload_started = true;
        state.clockface_upload_error = "";
        state.clockface_upload_size = 0;
        String face;
        const String asset = safe_clockface_asset_name(upload.filename);
        if (!requested_clockface_name(face) || !asset.length())
            state.clockface_upload_error = "Only PNG clock-face assets are accepted";
        else
        {
            LittleFS.mkdir("/clockface");
            LittleFS.mkdir((String("/clockface/") + face).c_str());
            state.clockface_upload_path =
                String("/clockface/") + face + "/" + asset;
            state.clockface_upload = LittleFS.open(
                state.clockface_upload_path.c_str(), "w");
            if (!state.clockface_upload)
                state.clockface_upload_error = "Could not create clock-face asset";
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (!state.clockface_upload_error.length() &&
            state.clockface_upload.write(upload.buf, upload.currentSize) !=
                upload.currentSize)
            state.clockface_upload_error = "Could not write clock-face asset";
        state.clockface_upload_size += upload.currentSize;
        if (state.clockface_upload_size > 1024U * 1024U)
            state.clockface_upload_error = "Clock-face assets are limited to 1 MB";
    }
    else if (upload.status == UPLOAD_FILE_END ||
             upload.status == UPLOAD_FILE_ABORTED)
    {
        if (state.clockface_upload)
            state.clockface_upload.close();
        if (upload.status == UPLOAD_FILE_ABORTED)
            state.clockface_upload_error = "Clock-face upload was cancelled";
        if (state.clockface_upload_error.length())
            LittleFS.remove(state.clockface_upload_path.c_str());
    }
}

static void finish_clockface_asset_upload()
{
    auto &state = active_control_panel->state();
    if (!state.clockface_upload_started || state.clockface_upload_error.length())
    {
        send_result(false, state.clockface_upload_error.length()
            ? state.clockface_upload_error.c_str()
            : "No clock-face asset was uploaded", 400);
        return;
    }
    state.clockface_upload_started = false;
    send_result(true, "Clock-face asset saved");
}

static void send_clockface_asset()
{
    String face;
    const String asset = safe_clockface_asset_name(g_server.arg("name"));
    if (!requested_clockface_name(face) || !asset.length())
    {
        send_result(false, "Clock-face asset not found", 404);
        return;
    }
    File file = LittleFS.open(
        (String("/clockface/") + face + "/" + asset).c_str(), "r");
    if (!file)
    {
        send_result(false, "Clock-face asset not found", 404);
        return;
    }
    send_clockface_file(file, "image/png");
    file.close();
}

void register_control_panel_clockface_routes_impl(WebServer &server) {
    server.on("/api/clockface/list", HTTP_GET, send_clockface_list);
    server.on("/api/clockface/fonts", HTTP_GET, send_clockface_fonts);
    server.on("/api/clockface/glyph", HTTP_GET, send_clockface_glyph);
    server.on("/api/clockface/project", HTTP_GET, send_clockface_project);
    server.on("/api/clockface/assets", HTTP_GET, send_clockface_assets);
    server.on("/api/clockface/project", HTTP_POST, save_clockface_project);
    server.on("/api/clockface/select", HTTP_POST, select_clockface_project);
    server.on("/api/clockface/asset", HTTP_GET, send_clockface_asset);
    server.on("/api/clockface/asset/upload", HTTP_POST, finish_clockface_asset_upload, receive_clockface_asset_upload);
}
} // namespace

String safe_clockface_name(const String &source)
{
    return safe_clockface_name_impl(source);
}

void register_control_panel_clockface_routes(WebServer &server)
{
    register_control_panel_clockface_routes_impl(server);
}
