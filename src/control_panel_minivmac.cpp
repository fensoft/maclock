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
    control_panel_queue_download_progress(progress_message, 0);
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
    control_panel_queue_download_progress(progress_message, 100);
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
            control_panel_queue_download_progress(
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
            control_panel_queue_download_progress(
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
    control_panel_queue_download_hide();

    auto &state = service->state();
    state.minivmac_bootstrap_task = nullptr;
    vTaskDelete(nullptr);
}

static void start_minivmac_bootstrap_impl(ControlPanelService &service)
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

void register_control_panel_minivmac_routes_impl(WebServer &server) {
    server.on("/api/minivmac/files", HTTP_GET, send_minivmac_files);
    server.on("/api/minivmac/download", HTTP_GET, download_minivmac_file);
    server.on("/api/minivmac/upload", HTTP_POST, finish_minivmac_upload, receive_minivmac_upload);
}
} // namespace

void start_minivmac_bootstrap(ControlPanelService &service)
{
    start_minivmac_bootstrap_impl(service);
}

void register_control_panel_minivmac_routes(WebServer &server)
{
    register_control_panel_minivmac_routes_impl(server);
}
