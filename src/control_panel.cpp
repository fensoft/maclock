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
};

namespace
{
ControlPanelService *active_control_panel = nullptr;

#define g_events (active_control_panel->state().events)
#define g_server (active_control_panel->state().server)
#define g_routes_ready (active_control_panel->state().routes_ready)
#define g_server_running \
    (active_control_panel->state().server_running)
#define g_mdns_running (active_control_panel->state().mdns_running)
#define g_sound_library \
    (active_control_panel->state().sound_library)
#define g_configuration_archive \
    (active_control_panel->state().configuration_archive)
#define g_update_upload_started \
    (active_control_panel->state().update_upload_started)
#define g_update_upload_finished \
    (active_control_panel->state().update_upload_finished)
#define g_update_upload_error \
    (active_control_panel->state().update_upload_error)

static void queue_download_progress(
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

static void queue_download_hide()
{
    auto &state = active_control_panel->state();
    if (state.progress_lock)
        xSemaphoreTake(state.progress_lock, portMAX_DELAY);
    state.progress_hide_pending = true;
    state.progress_dirty = false;
    if (state.progress_lock)
        xSemaphoreGive(state.progress_lock);
}

static void send_json(JsonDocument &document, int status = 200)
{
    String response;
    serializeJson(document, response);
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send(status, "application/json", response);
}

static void send_result(
    bool ok, const char *message, int status = 200)
{
    JsonDocument document;
    document["ok"] = ok;
    document["message"] = message;
    send_json(document, status);
}

static bool minivmac_slot(
    const String &slot, String &path, String &display_name)
{
    if (slot == "rom")
    {
        path = "/vMac.ROM";
        display_name = "vMac.ROM";
        return true;
    }
    if (!slot.startsWith("disk"))
        return false;
    const int number = atoi(slot.substring(4).c_str());
    if (number < 1 || number > 6 ||
        slot != String("disk") + number)
        return false;
    path = String("/disk") + number + ".dsk";
    display_name = String("disk") + number + ".dsk";
    return true;
}

static void send_minivmac_files()
{
    JsonDocument document;
    JsonArray files = document["files"].to<JsonArray>();
    for (int index = 0; index <= 6; ++index)
    {
        const String slot =
            index == 0 ? "rom" : String("disk") + index;
        String path;
        String name;
        minivmac_slot(slot, path, name);
        JsonObject entry = files.add<JsonObject>();
        entry["slot"] = slot;
        entry["name"] = name;
        entry["exists"] = LittleFS.exists(path.c_str());
        if (entry["exists"].as<bool>())
        {
            File file = LittleFS.open(path.c_str(), "r");
            entry["size"] = file ? file.size() : 0;
            file.close();
        }
        else
            entry["size"] = 0;
    }
    send_json(document);
}

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

static String safe_clockface_name(const String &source)
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
    name = safe_clockface_name(
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
                const String name = safe_clockface_name(
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
        clear ? "" : name.c_str());
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

static void download_minivmac_file()
{
    String path;
    String name;
    if (!g_server.hasArg("slot") ||
        !minivmac_slot(g_server.arg("slot"), path, name))
    {
        send_result(false, "Invalid Mini vMac file slot", 400);
        return;
    }
    File file = LittleFS.open(path.c_str(), "r");
    if (!file)
    {
        send_result(false, "Mini vMac file not found", 404);
        return;
    }
    g_server.sendHeader(
        "Content-Disposition",
        String("attachment; filename=\"") + name + "\"");
    g_server.sendHeader("Cache-Control", "no-store");
#if defined(MACLOCK_LOCAL)
    g_server.send(
        501, "application/json",
        "{\"ok\":false,\"message\":\"File downloads are unavailable locally\"}");
#else
    g_server.streamFile(file, "application/octet-stream");
#endif
    file.close();
}

static void receive_minivmac_upload()
{
    HTTPUpload &upload = g_server.upload();
    auto &state = active_control_panel->state();
    if (upload.status == UPLOAD_FILE_START)
    {
        state.minivmac_upload_error = "";
        state.minivmac_upload_size = 0;
        state.minivmac_upload_started = true;
        String name;
        if (!g_server.hasArg("slot") ||
            !minivmac_slot(
                g_server.arg("slot"),
                state.minivmac_upload_target, name))
        {
            state.minivmac_upload_error =
                "Invalid Mini vMac file slot";
            return;
        }
        state.minivmac_upload_temp =
            state.minivmac_upload_target + ".upload";
        LittleFS.remove(state.minivmac_upload_temp.c_str());
        state.minivmac_upload =
            LittleFS.open(state.minivmac_upload_temp.c_str(), "w");
        if (!state.minivmac_upload)
            state.minivmac_upload_error =
                "Could not create upload staging file";
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (!state.minivmac_upload_error.length() &&
            state.minivmac_upload.write(
                upload.buf, upload.currentSize) != upload.currentSize)
            state.minivmac_upload_error =
                "Not enough storage for this file";
        state.minivmac_upload_size += upload.currentSize;
    }
    else if (
        upload.status == UPLOAD_FILE_END ||
        upload.status == UPLOAD_FILE_ABORTED)
    {
        if (state.minivmac_upload)
            state.minivmac_upload.close();
        if (upload.status == UPLOAD_FILE_ABORTED)
            state.minivmac_upload_error = "Upload was cancelled";
    }
}

static void finish_minivmac_upload()
{
    auto &state = active_control_panel->state();
    if (!state.minivmac_upload_started)
    {
        send_result(false, "No Mini vMac upload was received", 400);
        return;
    }
    state.minivmac_upload_started = false;
    if (state.minivmac_upload)
        state.minivmac_upload.close();
    if (!state.minivmac_upload_error.length())
    {
        if (state.minivmac_upload_target == "/vMac.ROM" &&
            state.minivmac_upload_size != 128 * 1024)
            state.minivmac_upload_error =
                "The Macintosh Plus ROM must be exactly 128 KiB";
        else if (
            state.minivmac_upload_target != "/vMac.ROM" &&
            (state.minivmac_upload_size == 0 ||
             state.minivmac_upload_size % 512 != 0))
            state.minivmac_upload_error =
                "A disk image must be non-empty and 512-byte aligned";
    }
    if (state.minivmac_upload_error.length())
    {
        LittleFS.remove(state.minivmac_upload_temp.c_str());
        send_result(
            false, state.minivmac_upload_error.c_str(), 400);
        return;
    }

    const String backup = state.minivmac_upload_target + ".previous";
    LittleFS.remove(backup.c_str());
    const bool had_existing =
        LittleFS.exists(state.minivmac_upload_target.c_str());
    if (had_existing &&
        !LittleFS.rename(
            state.minivmac_upload_target.c_str(), backup.c_str()))
    {
        LittleFS.remove(state.minivmac_upload_temp.c_str());
        send_result(false, "Could not stage the existing file", 500);
        return;
    }
    if (!LittleFS.rename(
            state.minivmac_upload_temp.c_str(),
            state.minivmac_upload_target.c_str()))
    {
        if (had_existing)
            LittleFS.rename(
                backup.c_str(),
                state.minivmac_upload_target.c_str());
        LittleFS.remove(state.minivmac_upload_temp.c_str());
        send_result(false, "Could not install the uploaded file", 500);
        return;
    }
    if (had_existing)
        LittleFS.remove(backup.c_str());
    send_result(true, "Mini vMac file installed");
}

static bool download_to_littlefs(
    const char *url, const char *path,
    const char *progress_message)
{
    queue_download_progress(progress_message, 0);
#if defined(MACLOCK_LOCAL)
    NetworkClient client;
    HTTPClient http;
    if (!http.begin(client, url))
    {
        http.end();
        return false;
    }
    const int status = http.GET();
    if (status != HTTP_CODE_OK)
    {
        Serial.printf(
            "[Mini vMac] Download failed: HTTP %d (%s)\n",
            status, url);
        http.end();
        return false;
    }
    const String response = http.getString();
    queue_download_progress(progress_message, 100);
    Serial.printf(
        "[Mini vMac] Downloaded %lu bytes from %s\n",
        static_cast<unsigned long>(response.length()), url);
    http.end();
    File output = LittleFS.open(path, "w");
    const bool ok = output &&
        response.length() &&
        output.write(
            reinterpret_cast<const uint8_t *>(response.c_str()),
            response.length()) == response.length();
    output.close();
    if (!ok)
    {
        Serial.printf(
            "[Mini vMac] Could not stage %lu downloaded bytes at %s\n",
            static_cast<unsigned long>(response.length()), path);
        LittleFS.remove(path);
    }
    return ok;
#else
    WiFiClient client;
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, url) || http.GET() != HTTP_CODE_OK)
    {
        http.end();
        return false;
    }
    File output = LittleFS.open(path, FILE_WRITE);
    if (!output)
    {
        http.end();
        return false;
    }
    WiFiClient *stream = http.getStreamPtr();
    static constexpr size_t kDownloadBufferSize = 4096;
    uint8_t *buffer = static_cast<uint8_t *>(
        heap_caps_malloc(
            kDownloadBufferSize,
            MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM));
    if (!buffer)
        buffer = static_cast<uint8_t *>(
            heap_caps_malloc(
                kDownloadBufferSize, MALLOC_CAP_8BIT));
    if (!buffer)
    {
        output.close();
        http.end();
        LittleFS.remove(path);
        return false;
    }
    int remaining = http.getSize();
    const int total = remaining;
    int last_progress = -1;
    uint32_t last_data_ms = millis();
    bool ok = true;
    while (http.connected() && (remaining < 0 || remaining > 0))
    {
        const size_t available = stream->available();
        if (!available)
        {
            if (millis() - last_data_ms >= 15000)
            {
                ok = false;
                break;
            }
            delay(1);
            continue;
        }
        const size_t wanted =
            min(available, kDownloadBufferSize);
        const int count = stream->readBytes(buffer, wanted);
        if (count <= 0 ||
            output.write(buffer, count) != static_cast<size_t>(count))
        {
            ok = false;
            break;
        }
        if (remaining > 0)
            remaining -= count;
        last_data_ms = millis();
        const int progress =
            total > 0
                ? static_cast<int>(
                      (static_cast<uint64_t>(total - remaining) *
                       100) /
                      total)
                : 0;
        if (progress != last_progress)
        {
            last_progress = progress;
            queue_download_progress(
                progress_message, static_cast<uint8_t>(progress));
        }
        delay(0);
    }
    heap_caps_free(buffer);
    output.close();
    http.end();
    if (remaining > 0)
        ok = false;
    if (!ok)
        LittleFS.remove(path);
    return ok;
#endif
}

static uint16_t zip_u16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

static uint32_t zip_u32(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

static bool inflate_zip_entry(
    File &input, File &output, size_t compressed_size,
    size_t uncompressed_size)
{
#if defined(MACLOCK_LOCAL)
    std::vector<uint8_t> compressed(compressed_size);
    std::vector<uint8_t> uncompressed(uncompressed_size);
    if (input.read(
            compressed.data(), compressed.size()) != compressed.size())
        return false;
    z_stream stream = {};
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return false;
    stream.next_in = compressed.data();
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_out = uncompressed.data();
    stream.avail_out = static_cast<uInt>(uncompressed.size());
    const int result = inflate(&stream, Z_FINISH);
    const bool ok =
        result == Z_STREAM_END &&
        stream.total_out == uncompressed.size() &&
        output.write(
            uncompressed.data(), uncompressed.size()) ==
            uncompressed.size();
    if (!ok)
        Serial.printf(
            "[Mini vMac] ZIP inflate failed: result=%d, input=%lu/%lu, output=%lu/%lu\n",
            result,
            static_cast<unsigned long>(stream.total_in),
            static_cast<unsigned long>(compressed.size()),
            static_cast<unsigned long>(stream.total_out),
            static_cast<unsigned long>(uncompressed.size()));
    inflateEnd(&stream);
    return ok;
#else
    (void)uncompressed_size;
    auto allocate_buffer = [](size_t size) -> void *
    {
        void *buffer = heap_caps_malloc(
            size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        return buffer ? buffer : malloc(size);
    };
    auto *dictionary = static_cast<uint8_t *>(
        allocate_buffer(TINFL_LZ_DICT_SIZE));
    auto *compressed = static_cast<uint8_t *>(
        allocate_buffer(4096));
    auto *decompressor = static_cast<tinfl_decompressor *>(
        allocate_buffer(sizeof(tinfl_decompressor)));
    if (!dictionary || !compressed || !decompressor)
    {
        free(decompressor);
        free(compressed);
        free(dictionary);
        return false;
    }
    tinfl_init(decompressor);
    size_t remaining = compressed_size;
    size_t available = 0;
    size_t input_offset = 0;
    size_t output_offset = 0;
    tinfl_status status = TINFL_STATUS_NEEDS_MORE_INPUT;
    while (status != TINFL_STATUS_DONE)
    {
        if (!available)
        {
            const size_t count = min(remaining, size_t(4096));
            if (!count || input.read(compressed, count) != count)
                break;
            remaining -= count;
            available = count;
            input_offset = 0;
        }
        size_t consumed = available;
        size_t produced = TINFL_LZ_DICT_SIZE - output_offset;
        const mz_uint32 flags =
            remaining ? TINFL_FLAG_HAS_MORE_INPUT : 0;
        status = tinfl_decompress(
            decompressor, compressed + input_offset, &consumed,
            dictionary, dictionary + output_offset, &produced,
            flags);
        input_offset += consumed;
        available -= consumed;
        if (produced)
        {
            if (output.write(
                    dictionary + output_offset, produced) != produced)
                status = TINFL_STATUS_FAILED;
            output_offset =
                (output_offset + produced) % TINFL_LZ_DICT_SIZE;
        }
        if (status < TINFL_STATUS_DONE)
            break;
        delay(0);
    }
    free(decompressor);
    free(compressed);
    free(dictionary);
    const bool ok = status == TINFL_STATUS_DONE;
    if (!ok)
        Serial.printf(
            "[Mini vMac] ZIP inflate failed: status=%d, remaining=%lu, buffered=%lu\n",
            static_cast<int>(status),
            static_cast<unsigned long>(remaining),
            static_cast<unsigned long>(available));
    return ok;
#endif
}

static bool extract_system7(
    const char *zip_path, const char *disk_path)
{
    File input = LittleFS.open(zip_path, "r");
    if (!input)
    {
        Serial.println("[Mini vMac] Cannot open downloaded System 7 ZIP");
        return false;
    }
    bool local_header_found = false;
    uint8_t signature[4];
    const size_t search_limit = min(input.size(), size_t(4096));
    while (input.position() + sizeof(signature) <= search_limit)
    {
        const size_t start = input.position();
        if (input.read(signature, sizeof(signature)) !=
            sizeof(signature))
            break;
        if (zip_u32(signature) == 0x04034b50UL)
        {
            local_header_found = input.seek(start);
            break;
        }
        if (!input.seek(start + 1))
            break;
    }
    if (!local_header_found)
        Serial.println("[Mini vMac] System 7 ZIP header not found");
    bool ok = false;
    size_t size = 0;
    while (
        local_header_found &&
        input.size() - input.position() >= 30)
    {
        uint8_t header[30];
        const size_t header_size =
            input.read(header, sizeof(header));
        if (header_size != sizeof(header) ||
            zip_u32(header) != 0x04034b50UL)
            break;
        const uint16_t flags = zip_u16(header + 6);
        const uint16_t method = zip_u16(header + 8);
        const size_t compressed_size = zip_u32(header + 18);
        const size_t uncompressed_size = zip_u32(header + 22);
        const uint16_t name_length = zip_u16(header + 26);
        const uint16_t extra_length = zip_u16(header + 28);
        if ((flags & 0x08) || name_length >= 128)
            break;
        char name[128] = {};
        if (input.read(
                reinterpret_cast<uint8_t *>(name),
                name_length) != name_length ||
            !input.seek(input.position() + extra_length))
            break;
        if (strcasecmp(name, "System7.DSK") != 0)
        {
            if (!input.seek(input.position() + compressed_size))
                break;
            continue;
        }
        File output = LittleFS.open(disk_path, "w");
        if (!output)
            break;
        if (method == 0)
        {
            uint8_t buffer[4096];
            size_t remaining = compressed_size;
            ok = true;
            while (remaining)
            {
                const size_t count = min(remaining, sizeof(buffer));
                if (input.read(buffer, count) != count ||
                    output.write(buffer, count) != count)
                {
                    ok = false;
                    break;
                }
                remaining -= count;
            }
        }
        else if (method == 8)
            ok = inflate_zip_entry(
                input, output, compressed_size, uncompressed_size);
        output.close();
        File installed = LittleFS.open(disk_path, "r");
        size = installed ? installed.size() : 0;
        installed.close();
        break;
    }
    input.close();
    if (!ok || !size || size % 512 != 0)
    {
        Serial.printf(
            "[Mini vMac] System 7 extraction failed: ok=%d, size=%lu\n",
            ok, static_cast<unsigned long>(size));
        LittleFS.remove(disk_path);
        return false;
    }
    Serial.printf(
        "[Mini vMac] Extracted System 7 disk: %lu bytes\n",
        static_cast<unsigned long>(size));
    return true;
}

static void provision_minivmac_defaults()
{
    if (!LittleFS.exists("/vMac.ROM"))
    {
        Serial.println("[Mini vMac] Downloading missing ROM");
        if (download_to_littlefs(
                "http://psychonaut.bplaced.net/MinivMac/vMac.ROM",
                "/vMac.ROM.download",
                "Downloading Macintosh ROM..."))
        {
            File rom = LittleFS.open("/vMac.ROM.download", "r");
            const bool valid = rom && rom.size() >= 128 * 1024;
            rom.close();
            if (valid)
                LittleFS.rename("/vMac.ROM.download", "/vMac.ROM");
            else
                LittleFS.remove("/vMac.ROM.download");
        }
    }
    if (!LittleFS.exists("/disk1.dsk"))
    {
        Serial.println("[Mini vMac] Downloading missing System 7 disk");
        LittleFS.remove("/System7.zip.download");
        LittleFS.remove("/disk1.dsk.download");
        const bool downloaded = download_to_littlefs(
            "http://psychonaut.bplaced.net/MinivMac/MinivMac_disks/System7.zip",
            "/System7.zip.download",
            "Downloading System 7 disk...");
        if (downloaded)
            queue_download_progress(
                "Installing System 7 disk...", 100);
        if (!downloaded)
            Serial.println("[Mini vMac] System 7 download failed");
        else if (extract_system7(
                     "/System7.zip.download", "/disk1.dsk.download"))
        {
            if (LittleFS.rename(
                    "/disk1.dsk.download", "/disk1.dsk"))
                Serial.println("[Mini vMac] System 7 disk installed");
            else
                Serial.println("[Mini vMac] Could not install System 7 disk");
        }
        LittleFS.remove("/System7.zip.download");
        LittleFS.remove("/disk1.dsk.download");
    }
}

static void minivmac_bootstrap_task(void *context)
{
    auto *service = static_cast<ControlPanelService *>(context);
    active_control_panel = service;
    provision_minivmac_defaults();
    queue_download_hide();

    auto &state = service->state();
    state.minivmac_bootstrap_task = nullptr;
    vTaskDelete(nullptr);
}

static void start_minivmac_bootstrap(ControlPanelService &service)
{
    auto &state = service.state();
    if (state.minivmac_bootstrap_attempted ||
        state.minivmac_bootstrap_task)
        return;

    state.minivmac_bootstrap_attempted = true;
    if (LittleFS.exists("/vMac.ROM") &&
        LittleFS.exists("/disk1.dsk"))
        return;

    const BaseType_t created = xTaskCreatePinnedToCore(
        minivmac_bootstrap_task, "minivmac_assets", 12288,
        &service, 1, &state.minivmac_bootstrap_task, 0);
    if (created != pdPASS)
    {
        state.minivmac_bootstrap_task = nullptr;
        Serial.println("[Mini vMac] Could not start asset worker");
    }
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

static bool read_uint(
    const char *name, uint32_t minimum, uint32_t maximum,
    uint32_t &value)
{
    if (!g_server.hasArg(name))
        return false;

    const String text = g_server.arg(name);
    if (!text.length())
        return false;
    for (const char character : text)
    {
        if (character < '0' || character > '9')
            return false;
    }

    const unsigned long parsed = strtoul(text.c_str(), nullptr, 10);
    if (parsed < minimum || parsed > maximum)
        return false;
    value = static_cast<uint32_t>(parsed);
    return true;
}

static bool read_optional_uint(
    const char *name, uint32_t minimum, uint32_t maximum,
    uint32_t &value)
{
    return !g_server.hasArg(name) ||
           read_uint(name, minimum, maximum, value);
}

static bool read_sound_arg(const char *name, String &sound)
{
    if (!g_server.hasArg(name))
        return false;
    sound = g_server.arg(name);
    for (size_t i = 0; i < SoundSelector::count(); ++i)
    {
        const char *path = SoundSelector::pathAt(i);
        if (path && sound.equalsIgnoreCase(path))
        {
            sound = path;
            return true;
        }
    }
    return false;
}

static bool read_sound(String &sound)
{
    return read_sound_arg("sound", sound);
}

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

static void append_update_json(
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

static void append_mqtt_json(
    JsonObject mqtt, const MqttSnapshot &snapshot)
{
    mqtt["enabled"] = snapshot.settings.enabled;
    mqtt["host"] = snapshot.settings.host;
    mqtt["port"] = snapshot.settings.port;
    mqtt["username"] = snapshot.settings.username;
    mqtt["passwordSet"] = snapshot.password_set;
    mqtt["connected"] = snapshot.connected;
    mqtt["status"] = snapshot.status;
    mqtt["deviceId"] = snapshot.device_id;
    mqtt["topicBase"] = snapshot.topic_base;
    mqtt["displayState"] = snapshot.display_state;
    mqtt["currentId"] = snapshot.current_id;
    mqtt["pendingId"] = snapshot.pending_id;
    mqtt["lastId"] = snapshot.last_id;
    mqtt["lastResult"] = snapshot.last_result;
    mqtt["lastError"] = snapshot.last_error;
    mqtt["sound"] = snapshot.sound;
    mqtt["soundVolume"] = snapshot.sound_volume;
    mqtt["backlight"] = snapshot.backlight;
    mqtt["doNotDisturb"] = snapshot.do_not_disturb;
    mqtt["timerActive"] = snapshot.timer_active;
    mqtt["screensaver"] = snapshot.screensaver;
    mqtt["wifiRssi"] = snapshot.wifi_rssi;
    mqtt["firmwareVersion"] = snapshot.firmware_version;
    mqtt["temperatureValid"] = snapshot.temperature_valid;
    mqtt["temperature"] = snapshot.temperature;
}

static void send_control_page()
{
    g_server.sendHeader(
        "Cache-Control", "no-store, no-cache, must-revalidate");
    g_server.sendHeader("Content-Encoding", "gzip");
    g_server.send_P(
        200, "text/html",
        reinterpret_cast<PGM_P>(kControlPanelPageGzip),
        kControlPanelPageGzipLength);
}

static void send_state()
{
    if (!g_events)
    {
        send_result(false, "Control service is unavailable", 503);
        return;
    }

    const ControlPanelSnapshot snapshot =
        g_events->controlPanelSnapshot();
    JsonDocument document;

    JsonObject appearance =
        document["appearance"].to<JsonObject>();
    document["touchscreenPresent"] =
        snapshot.touchscreen_present;
    appearance["language"] =
        static_cast<uint8_t>(snapshot.settings.language);
    appearance["customClockFace"] = snapshot.settings.custom_clock_face;
    appearance["animationSpeed"] = static_cast<uint8_t>(
        snapshot.settings.face_customization.flip_speed);
    appearance["colonBlink"] = static_cast<uint8_t>(
        snapshot.settings.face_customization.colon_blink);
    appearance["continuousSeconds"] =
        snapshot.settings.face_customization.continuous_seconds;
    appearance["brightness"] = snapshot.brightness;
    appearance["hourFormat"] =
        static_cast<uint8_t>(
            snapshot.settings.time_format.hour_format);
    appearance["showSeconds"] =
        snapshot.settings.time_format.show_seconds;

    JsonObject screensaver =
        document["screensaver"].to<JsonObject>();
    screensaver["mode"] =
        static_cast<uint8_t>(
            snapshot.settings.screensaver_mode);
    screensaver["delay"] =
        snapshot.settings.screensaver_delay_index;
    screensaver["active"] =
        snapshot.screensaver_active;

    JsonObject system_sounds =
        document["systemSounds"].to<JsonObject>();
    system_sounds["startup"] = snapshot.startup_sound;
    system_sounds["startupVolume"] =
        snapshot.startup_volume;
    system_sounds["floppy"] = snapshot.floppy_sound;
    system_sounds["floppyVolume"] =
        snapshot.floppy_volume;

    JsonObject night = document["night"].to<JsonObject>();
    night["enabled"] = snapshot.settings.night_mode.enabled;
    night["start"] = snapshot.settings.night_mode.start_hour;
    night["end"] = snapshot.settings.night_mode.end_hour;
    night["screenOff"] =
        snapshot.settings.night_mode.screen_off_enabled;
    night["offHour"] =
        snapshot.settings.night_mode.screen_off_hour;

    JsonObject chime = document["chime"].to<JsonObject>();
    chime["mode"] =
        static_cast<uint8_t>(snapshot.settings.chime.mode);
    chime["sound"] = snapshot.chime_sound;
    chime["volume"] = snapshot.settings.chime.volume;
    chime["quiet"] = snapshot.settings.chime.quiet_enabled;
    chime["quietStart"] =
        snapshot.settings.chime.quiet_start_hour;
    chime["quietEnd"] =
        snapshot.settings.chime.quiet_end_hour;

    JsonArray alarms = document["alarms"].to<JsonArray>();
    for (const ControlPanelAlarm &alarm : snapshot.alarms)
    {
        JsonObject item = alarms.add<JsonObject>();
        item["enabled"] = alarm.enabled != 0;
        item["hour"] = alarm.hour;
        item["minute"] = alarm.minute;
        item["weekdays"] = alarm.weekdays;
        item["sound"] = alarm.sound;
        item["volume"] = alarm.volume;
        item["oneTime"] = alarm.one_time;
        item["gradualVolume"] = alarm.gradual_volume;
        item["sunrise"] = alarm.sunrise;
        item["label"] = alarm.label;
    }
    JsonObject upcoming =
        document["upcomingAlarm"].to<JsonObject>();
    upcoming["valid"] = snapshot.upcoming_alarm.valid;
    upcoming["snoozed"] =
        snapshot.upcoming_alarm.snoozed;
    upcoming["oneTime"] =
        snapshot.upcoming_alarm.one_time;
    upcoming["index"] = snapshot.upcoming_alarm.index;
    upcoming["dayOffset"] =
        snapshot.upcoming_alarm.day_offset;
    upcoming["weekday"] =
        snapshot.upcoming_alarm.weekday;
    upcoming["hour"] = snapshot.upcoming_alarm.hour;
    upcoming["minute"] = snapshot.upcoming_alarm.minute;
    upcoming["label"] = snapshot.upcoming_alarm.label;

    JsonObject timer = document["timer"].to<JsonObject>();
    timer["active"] = snapshot.timer.active;
    timer["minutes"] = snapshot.timer.minutes;
    timer["remaining"] = snapshot.timer.remaining_seconds;
    timer["sound"] = snapshot.timer.sound;
    timer["volume"] = snapshot.timer.volume;

    JsonObject location =
        document["location"].to<JsonObject>();
    location["city"] = snapshot.location.city;
    location["country"] = snapshot.location.country;
    location["resolved"] = snapshot.location.resolved;
    location["timezone"] = snapshot.location.timezone;

    JsonArray sounds = document["sounds"].to<JsonArray>();
    g_sound_library.appendSnapshot(sounds, snapshot);
    const size_t filesystem_total = LittleFS.totalBytes();
    const size_t filesystem_used = LittleFS.usedBytes();
    JsonObject storage =
        document["storage"].to<JsonObject>();
    storage["total"] = filesystem_total;
    storage["used"] = filesystem_used;
    storage["free"] =
        filesystem_total > filesystem_used
            ? filesystem_total - filesystem_used
            : 0;
    append_update_json(
        document["update"].to<JsonObject>(), snapshot.update);
    append_mqtt_json(
        document["mqtt"].to<JsonObject>(), snapshot.mqtt);

    send_json(document);
}

static void send_status()
{
    if (!g_events)
    {
        send_result(false, "Control service is unavailable", 503);
        return;
    }

    const ControlPanelSnapshot snapshot =
        g_events->controlPanelSnapshot();
    JsonDocument document;
    document["timer"]["active"] = snapshot.timer.active;
    document["timer"]["remaining"] =
        snapshot.timer.remaining_seconds;
    document["screensaver"]["active"] =
        snapshot.screensaver_active;
    JsonObject upcoming =
        document["upcomingAlarm"].to<JsonObject>();
    upcoming["valid"] = snapshot.upcoming_alarm.valid;
    upcoming["snoozed"] =
        snapshot.upcoming_alarm.snoozed;
    upcoming["oneTime"] =
        snapshot.upcoming_alarm.one_time;
    upcoming["index"] = snapshot.upcoming_alarm.index;
    upcoming["dayOffset"] =
        snapshot.upcoming_alarm.day_offset;
    upcoming["weekday"] =
        snapshot.upcoming_alarm.weekday;
    upcoming["hour"] = snapshot.upcoming_alarm.hour;
    upcoming["minute"] = snapshot.upcoming_alarm.minute;
    upcoming["label"] = snapshot.upcoming_alarm.label;
    append_update_json(
        document["update"].to<JsonObject>(), snapshot.update);
    append_mqtt_json(
        document["mqtt"].to<JsonObject>(), snapshot.mqtt);
    send_json(document);
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
    append_update_json(
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

static void apply_appearance()
{
    uint32_t language = 0;
    uint32_t brightness = 0;
    uint32_t hour_format = 0;
    uint32_t show_seconds = 0;
    FaceCustomizationSettings face_customization;
    String custom_clock_face;
    uint32_t flip_speed =
        static_cast<uint8_t>(face_customization.flip_speed);
    uint32_t colon_blink =
        static_cast<uint8_t>(face_customization.colon_blink);
    uint32_t continuous_seconds =
        face_customization.continuous_seconds ? 1 : 0;
    if (!read_uint(
            "language", 0, UI_LANGUAGE_COUNT - 1, language) ||
        !read_uint("brightness", 0, kBrightnessMax, brightness) ||
         !read_uint(
             "hourFormat", 0,
             static_cast<uint8_t>(HourFormat::Count) - 1,
             hour_format) ||
         !read_uint("showSeconds", 0, 1, show_seconds) ||
        !read_uint(
            "animationSpeed", 0,
             static_cast<uint8_t>(
                 FlipAnimationSpeed::Count) -
                 1,
            flip_speed) ||
         !read_uint("colonBlink", 0,
             static_cast<uint8_t>(ColonBlinkInterval::Count) - 1,
             colon_blink) ||
         !read_uint("continuousSeconds", 0, 1, continuous_seconds))
    {
        send_result(false, "Invalid appearance settings", 400);
        return;
    }
    if (g_server.hasArg("customClockFace"))
        custom_clock_face = safe_clockface_name(g_server.arg("customClockFace"));
    if (g_server.hasArg("customClockFace") &&
        g_server.arg("customClockFace").length() && !custom_clock_face.length())
    {
        send_result(false, "Invalid custom clock face", 400);
        return;
    }

    TimeFormatSettings time_format;
    time_format.hour_format =
        static_cast<HourFormat>(hour_format);
    time_format.show_seconds = show_seconds != 0;
    face_customization.flip_speed =
        static_cast<FlipAnimationSpeed>(flip_speed);
    face_customization.colon_blink =
        static_cast<ColonBlinkInterval>(colon_blink);
    face_customization.continuous_seconds = continuous_seconds != 0;
    const bool applied = g_events &&
        g_events->applyControlAppearance(
            static_cast<UiLanguage>(language),
            static_cast<uint8_t>(brightness),
            face_customization,
            time_format,
            custom_clock_face.c_str());
    send_result(
        applied,
        applied ? "Appearance updated" : "Appearance was not updated",
        applied ? 200 : 500);
}

static void apply_screensaver()
{
    uint32_t mode = 0;
    uint32_t delay = 0;
    const String action = g_server.arg("action");
    const bool launch_now = action == "launch";
    if ((!launch_now && action != "save") ||
        !read_uint(
            "mode", 0,
            static_cast<uint8_t>(
                ScreensaverMode::Count) -
                1,
            mode) ||
        !read_uint(
            "delay", 0,
            kScreensaverDelayCount - 1,
            delay))
    {
        send_result(
            false, "Invalid screensaver settings", 400);
        return;
    }

    const bool applied = g_events &&
        g_events->applyControlScreensaver(
            static_cast<ScreensaverMode>(mode),
            static_cast<uint8_t>(delay),
            launch_now);
    send_result(
        applied,
        applied
            ? (launch_now
                   ? "Screensaver launched"
                   : "Screensaver settings saved")
            : "Screensaver settings were not applied",
        applied ? 200 : 500);
}

#ifdef MACLOCK_LOCAL
static void show_local_manual_page()
{
    uint32_t page = 0;
    if (!read_uint("page", 0, 20, page) || !g_events ||
        !g_events->showLocalManualPage(static_cast<uint8_t>(page)))
    {
        send_result(false, "Invalid manual page", 400);
        return;
    }
    send_result(true, "Manual page displayed", 200);
}
#endif

static void apply_location()
{
    String city = g_server.arg("city");
    String country = g_server.arg("country");
    city.trim();
    country.trim();
    country.toUpperCase();
    const bool country_valid =
        !country.length() ||
        (country.length() == 2 &&
         country[0] >= 'A' && country[0] <= 'Z' &&
         country[1] >= 'A' && country[1] <= 'Z');
    if (!city.length() || city.length() > 48 ||
        !country_valid)
    {
        send_result(false, "Invalid location settings", 400);
        return;
    }

    const bool applied = g_events &&
        g_events->applyControlLocation(
            city.c_str(), country.c_str());
    send_result(
        applied,
        applied ? "Location update started"
                : "Location was not updated",
        applied ? 200 : 500);
}

static void apply_mqtt()
{
    uint32_t enabled = 0;
    uint32_t port = 0;
    uint32_t clear_password = 0;
    String host = g_server.arg("host");
    String username = g_server.arg("username");
    String password = g_server.arg("password");
    host.trim();
    username.trim();

    if (!read_uint("enabled", 0, 1, enabled) ||
        !read_uint("port", 1, 65535, port) ||
        !read_uint("clearPassword", 0, 1, clear_password) ||
        host.length() > kMqttHostMaxLength ||
        username.length() > kMqttUsernameMaxLength ||
        password.length() > kMqttPasswordMaxLength ||
        (enabled != 0 && !host.length()))
    {
        send_result(false, "Invalid MQTT settings", 400);
        return;
    }

    MqttSettings settings;
    settings.enabled = enabled != 0;
    settings.port = static_cast<uint16_t>(port);
    strlcpy(settings.host, host.c_str(), sizeof(settings.host));
    strlcpy(
        settings.username, username.c_str(),
        sizeof(settings.username));
    const char *new_password =
        password.length() ? password.c_str() : nullptr;
    const bool applied = g_events &&
        g_events->applyControlMqtt(
            settings, new_password, clear_password != 0);
    send_result(
        applied,
        applied ? "MQTT settings saved"
                : "MQTT settings were not saved",
        applied ? 200 : 500);
}

static void apply_alarm()
{
    uint32_t index = 0;
    uint32_t enabled = 0;
    uint32_t hour = 0;
    uint32_t minute = 0;
    uint32_t weekdays = 0;
    uint32_t volume = 0;
    uint32_t one_time = 0;
    uint32_t gradual_volume = 0;
    uint32_t sunrise = 0;
    String sound;
    String label = g_server.arg("label");
    label.trim();
    if (!read_uint(
            "index", 0, kControlPanelAlarmCount - 1, index) ||
        !read_uint("enabled", 0, 1, enabled) ||
        !read_uint("hour", 0, 23, hour) ||
        !read_uint("minute", 0, 59, minute) ||
        !read_uint("weekdays", 0, 0x7F, weekdays) ||
        !read_uint(
            "volume", 0, kAudioVolumeLevelCount - 1, volume) ||
        !read_uint("oneTime", 0, 1, one_time) ||
        !read_uint(
            "gradualVolume", 0, 1, gradual_volume) ||
        !read_uint("sunrise", 0, 1, sunrise) ||
        label.length() > kAlarmLabelMaxLength ||
        !read_sound(sound))
    {
        send_result(false, "Invalid alarm settings", 400);
        return;
    }

    ControlPanelAlarm alarm;
    alarm.enabled = static_cast<uint8_t>(enabled);
    alarm.hour = static_cast<uint8_t>(hour);
    alarm.minute = static_cast<uint8_t>(minute);
    alarm.weekdays = static_cast<uint8_t>(weekdays);
    alarm.volume = static_cast<uint8_t>(volume);
    alarm.one_time = one_time != 0;
    alarm.gradual_volume = gradual_volume != 0;
    alarm.sunrise = sunrise != 0;
    strlcpy(
        alarm.label, label.c_str(), sizeof(alarm.label));
    strlcpy(alarm.sound, sound.c_str(), sizeof(alarm.sound));
    const bool applied = g_events &&
        g_events->applyControlAlarm(index, alarm);
    send_result(
        applied,
        applied ? "Alarm saved" : "Alarm was not saved",
        applied ? 200 : 500);
}

static void apply_timer()
{
    uint32_t minutes = 0;
    uint32_t volume = 0;
    String sound;
    const String action = g_server.arg("action");
    const bool start = action == "start";
    const bool cancel = action == "cancel";
    if ((!start && !cancel && action != "save") ||
        !read_uint("minutes", 1, 1440, minutes) ||
        !read_uint(
            "volume", 0, kAudioVolumeLevelCount - 1, volume) ||
        !read_sound(sound))
    {
        send_result(false, "Invalid timer settings", 400);
        return;
    }

    ControlPanelTimer timer;
    timer.minutes = static_cast<uint16_t>(minutes);
    timer.volume = static_cast<uint8_t>(volume);
    strlcpy(timer.sound, sound.c_str(), sizeof(timer.sound));
    const bool applied = g_events &&
        g_events->applyControlTimer(timer, start, cancel);
    send_result(
        applied,
        cancel ? "Timer cancelled"
               : (start ? "Timer started" : "Timer defaults saved"),
        applied ? 200 : 500);
}

static void apply_night()
{
    uint32_t enabled = 0;
    uint32_t start = 0;
    uint32_t end = 0;
    uint32_t screen_off = 0;
    uint32_t off_hour = 0;
    if (!read_uint("enabled", 0, 1, enabled) ||
        !read_uint("start", 0, 23, start) ||
        !read_uint("end", 0, 23, end) ||
        !read_uint("screenOff", 0, 1, screen_off) ||
        !read_uint("offHour", 0, 23, off_hour))
    {
        send_result(false, "Invalid night-mode settings", 400);
        return;
    }

    NightModeSettings settings;
    settings.enabled = enabled != 0;
    settings.start_hour = static_cast<uint8_t>(start);
    settings.end_hour = static_cast<uint8_t>(end);
    settings.screen_off_enabled = screen_off != 0;
    settings.screen_off_hour = static_cast<uint8_t>(off_hour);
    const bool applied = g_events &&
        g_events->applyControlNightMode(settings);
    send_result(
        applied,
        applied ? "Night mode saved" : "Night mode was not saved",
        applied ? 200 : 500);
}

static void apply_chime()
{
    uint32_t mode = 0;
    uint32_t volume = 0;
    uint32_t quiet = 0;
    uint32_t quiet_start = 0;
    uint32_t quiet_end = 0;
    String sound;
    if (!read_uint(
            "mode", 0,
            static_cast<uint8_t>(ChimeMode::Count) - 1, mode) ||
        !read_uint(
            "volume", 0, kAudioVolumeLevelCount - 1, volume) ||
        !read_uint("quiet", 0, 1, quiet) ||
        !read_uint("quietStart", 0, 23, quiet_start) ||
        !read_uint("quietEnd", 0, 23, quiet_end) ||
        !read_sound(sound))
    {
        send_result(false, "Invalid chime settings", 400);
        return;
    }

    ChimeSettings settings;
    settings.mode = static_cast<ChimeMode>(mode);
    settings.volume = static_cast<uint8_t>(volume);
    settings.quiet_enabled = quiet != 0;
    settings.quiet_start_hour =
        static_cast<uint8_t>(quiet_start);
    settings.quiet_end_hour =
        static_cast<uint8_t>(quiet_end);
    const bool applied = g_events &&
        g_events->applyControlChime(settings, sound.c_str());
    send_result(
        applied,
        applied ? "Chime settings saved"
                : "Chime settings were not saved",
        applied ? 200 : 500);
}

static void preview_sound()
{
    uint32_t volume = 0;
    String sound;
    if (!read_uint("volume", 10, 100, volume) ||
        !audio_volume_is_level(static_cast<uint8_t>(volume)) ||
        !read_sound(sound))
    {
        send_result(false, "Invalid sound preview", 400);
        return;
    }

    const bool started = g_events &&
        g_events->previewControlSound(
            sound.c_str(), static_cast<uint8_t>(volume));
    send_result(
        started,
        started ? "Playing sound" : "Sound could not be played",
        started ? 200 : 500);
}

static void apply_system_sounds()
{
    uint32_t startup_volume = 0;
    uint32_t floppy_volume = 0;
    String startup;
    String floppy;
    if (!read_sound_arg("startup", startup) ||
        !read_uint(
            "startupVolume", 10, 100, startup_volume) ||
        !read_sound_arg("floppy", floppy) ||
        !read_uint(
            "floppyVolume", 10, 100, floppy_volume) ||
        !audio_volume_is_level(
            static_cast<uint8_t>(startup_volume)) ||
        !audio_volume_is_level(
            static_cast<uint8_t>(floppy_volume)))
    {
        send_result(false, "Invalid system sound settings", 400);
        return;
    }

    const bool applied = g_events &&
        g_events->applyControlSystemSounds(
            startup.c_str(),
            static_cast<uint8_t>(startup_volume),
            floppy.c_str(),
            static_cast<uint8_t>(floppy_volume));
    send_result(
        applied,
        applied ? "System sounds saved"
                : "System sounds were not saved",
        applied ? 200 : 500);
}

static void configure_routes()
{
    if (g_routes_ready)
        return;
    g_server.on("/", HTTP_GET, send_control_page);
    g_server.on("/api/state", HTTP_GET, send_state);
    g_server.on("/api/status", HTTP_GET, send_status);
    g_server.on("/api/clockface/list", HTTP_GET, send_clockface_list);
    g_server.on("/api/clockface/fonts", HTTP_GET, send_clockface_fonts);
    g_server.on("/api/clockface/glyph", HTTP_GET, send_clockface_glyph);
    g_server.on("/api/clockface/project", HTTP_GET, send_clockface_project);
    g_server.on("/api/clockface/assets", HTTP_GET, send_clockface_assets);
    g_server.on("/api/clockface/project", HTTP_POST, save_clockface_project);
    g_server.on("/api/clockface/select", HTTP_POST, select_clockface_project);
    g_server.on("/api/clockface/asset", HTTP_GET, send_clockface_asset);
    g_server.on(
        "/api/clockface/asset/upload", HTTP_POST,
        finish_clockface_asset_upload,
        receive_clockface_asset_upload);
    g_server.on(
        "/api/update/status", HTTP_GET, send_update_status);
    g_server.on("/api/appearance", HTTP_POST, apply_appearance);
#ifdef MACLOCK_LOCAL
    g_server.on(
        "/api/manual/page", HTTP_POST,
        show_local_manual_page);
#endif
    g_server.on("/api/location", HTTP_POST, apply_location);
    g_server.on("/api/mqtt", HTTP_POST, apply_mqtt);
    g_server.on(
        "/api/screensaver",
        HTTP_POST,
        apply_screensaver);
    g_server.on(
        "/api/screensaver/photos", HTTP_GET,
        send_screensaver_photos);
    g_server.on(
        "/api/screensaver/photo", HTTP_GET,
        send_screensaver_photo);
    g_server.on(
        "/api/screensaver/photo/upload", HTTP_POST,
        finish_screensaver_photo_upload,
        receive_screensaver_photo_upload);
    g_server.on(
        "/api/screensaver/photo/delete", HTTP_POST,
        delete_screensaver_photo);
    g_server.on("/api/alarm", HTTP_POST, apply_alarm);
    g_server.on("/api/timer", HTTP_POST, apply_timer);
    g_server.on("/api/night", HTTP_POST, apply_night);
    g_server.on("/api/chime", HTTP_POST, apply_chime);
    g_server.on("/api/sounds", HTTP_POST, apply_system_sounds);
    g_server.on("/api/preview", HTTP_POST, preview_sound);
    g_server.on(
        "/api/update/check", HTTP_POST, request_update_check);
    g_server.on(
        "/api/update/install", HTTP_POST, request_update_install);
    g_server.on(
        "/api/update/dismiss", HTTP_POST, dismiss_update);
    g_server.on(
        "/api/update/reboot", HTTP_POST, reboot_after_update);
    g_server.on(
        "/api/update/firmware", HTTP_POST,
        finish_firmware_upload, receive_firmware_upload);
    g_server.on(
        "/api/minivmac/files", HTTP_GET, send_minivmac_files);
    g_server.on(
        "/api/minivmac/download", HTTP_GET,
        download_minivmac_file);
    g_server.on(
        "/api/minivmac/upload", HTTP_POST,
        finish_minivmac_upload, receive_minivmac_upload);
    g_server.on(
        "/api/configuration/export", HTTP_GET,
        []()
        {
            g_configuration_archive.sendExport(g_server);
        });
    g_server.on(
        "/api/configuration/import", HTTP_POST,
        []()
        {
            g_configuration_archive.finishUpload(g_server);
        },
        []()
        {
            g_configuration_archive.receiveUpload(g_server);
        });
    g_server.on(
        "/api/sound/upload", HTTP_POST,
        []()
        {
            g_sound_library.finishUpload(g_server);
        },
        []()
        {
            g_sound_library.receiveUpload(g_server);
        });
    g_server.on(
        "/api/sound/import", HTTP_POST,
        []()
        {
            if (!g_events)
            {
                send_result(
                    false, "Control service is unavailable", 503);
                return;
            }
            g_sound_library.importFromUrl(g_server, *g_events);
        });
    g_server.on(
        "/api/sound/myinstants/search", HTTP_POST,
        []()
        {
            if (!g_events)
            {
                send_result(
                    false, "Control service is unavailable", 503);
                return;
            }
            g_sound_library.searchMyInstants(g_server, *g_events);
        });
    g_server.on(
        "/api/sound/delete", HTTP_POST,
        []()
        {
            if (!g_events)
            {
                send_result(
                    false, "Control service is unavailable", 503);
                return;
            }
            g_sound_library.remove(g_server, *g_events);
        });
    g_server.onNotFound(
        []()
        {
            send_result(false, "Control-panel route not found", 404);
        });
    g_routes_ready = true;
}
} // namespace

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
    configure_routes();
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
