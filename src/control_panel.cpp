#include "control_panel.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include <vector>
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
    if (g_events)
        g_events->showControlPanelDownload(
            progress_message, 0);
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
    if (g_events)
        g_events->showControlPanelDownload(
            progress_message, 100);
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
        if (g_events && progress != last_progress)
        {
            last_progress = progress;
            g_events->showControlPanelDownload(
                progress_message,
                static_cast<uint8_t>(progress));
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
        if (downloaded && g_events)
            g_events->showControlPanelDownload(
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
    mqtt["clockFace"] = snapshot.clock_face;
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
    appearance["language"] =
        static_cast<uint8_t>(snapshot.settings.language);
    appearance["face"] =
        static_cast<uint8_t>(snapshot.settings.clock_face);
    appearance["theme"] =
        static_cast<uint8_t>(snapshot.settings.clock_theme);
    appearance["accent"] = static_cast<uint8_t>(
        snapshot.settings.face_customization.accent);
    appearance["fontSize"] = static_cast<uint8_t>(
        snapshot.settings.face_customization.numeral_size);
    appearance["weather"] =
        snapshot.settings.face_customization.show_weather;
    appearance["flipSpeed"] = static_cast<uint8_t>(
        snapshot.settings.face_customization.flip_speed);
    appearance["brightness"] = snapshot.brightness;
    appearance["hourFormat"] =
        static_cast<uint8_t>(
            snapshot.settings.time_format.hour_format);
    appearance["leadingZero"] =
        snapshot.settings.time_format.leading_zero;
    appearance["seconds"] =
        snapshot.settings.time_format.show_seconds;
    appearance["weekday"] =
        snapshot.settings.time_format.show_weekday;

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
    uint32_t face = 0;
    uint32_t theme = 0;
    uint32_t brightness = 0;
    uint32_t hour_format = 0;
    uint32_t leading_zero = 0;
    uint32_t seconds = 0;
    uint32_t weekday = 0;
    FaceCustomizationSettings face_customization;
    if (g_events)
    {
        face_customization =
            g_events->controlPanelSnapshot()
                .settings.face_customization;
    }
    uint32_t accent =
        static_cast<uint8_t>(face_customization.accent);
    uint32_t font_size =
        static_cast<uint8_t>(face_customization.numeral_size);
    uint32_t weather = face_customization.show_weather ? 1 : 0;
    uint32_t flip_speed =
        static_cast<uint8_t>(face_customization.flip_speed);
    if (!read_uint(
            "language", 0, UI_LANGUAGE_COUNT - 1, language) ||
        !read_uint(
            "face", 0,
            static_cast<uint8_t>(ClockFace::Count) - 1, face) ||
        !read_uint(
            "theme", 0,
            static_cast<uint8_t>(ClockTheme::Count) - 1, theme) ||
        !read_uint("brightness", 0, kBrightnessMax, brightness) ||
        !read_uint(
            "hourFormat", 0,
            static_cast<uint8_t>(HourFormat::Count) - 1,
            hour_format) ||
        !read_uint("leadingZero", 0, 1, leading_zero) ||
        !read_uint("seconds", 0, 1, seconds) ||
        !read_uint("weekday", 0, 1, weekday) ||
        !read_optional_uint(
            "accent", 0,
            static_cast<uint8_t>(FaceAccent::Count) - 1,
            accent) ||
        !read_optional_uint(
            "fontSize", 0,
            static_cast<uint8_t>(FaceNumeralSize::Count) - 1,
            font_size) ||
        !read_optional_uint("weather", 0, 1, weather) ||
        !read_optional_uint(
            "flipSpeed", 0,
            static_cast<uint8_t>(
                FlipAnimationSpeed::Count) -
                1,
            flip_speed))
    {
        send_result(false, "Invalid appearance settings", 400);
        return;
    }

    TimeFormatSettings time_format;
    time_format.hour_format =
        static_cast<HourFormat>(hour_format);
    time_format.leading_zero = leading_zero != 0;
    time_format.show_seconds = seconds != 0;
    time_format.show_weekday = weekday != 0;
    face_customization.accent =
        static_cast<FaceAccent>(accent);
    face_customization.numeral_size =
        static_cast<FaceNumeralSize>(font_size);
    face_customization.show_weather = weather != 0;
    face_customization.flip_speed =
        static_cast<FlipAnimationSpeed>(flip_speed);
    const bool applied = g_events &&
        g_events->applyControlAppearance(
            static_cast<UiLanguage>(language),
            static_cast<ClockFace>(face),
            static_cast<ClockTheme>(theme),
            static_cast<uint8_t>(brightness),
            face_customization,
            time_format);
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
    g_server.on(
        "/api/update/status", HTTP_GET, send_update_status);
    g_server.on("/api/appearance", HTTP_POST, apply_appearance);
    g_server.on("/api/location", HTTP_POST, apply_location);
    g_server.on("/api/mqtt", HTTP_POST, apply_mqtt);
    g_server.on(
        "/api/screensaver",
        HTTP_POST,
        apply_screensaver);
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
    g_events = &events;
    g_configuration_archive.begin(events);
    configure_routes();
#if defined(MACLOCK_LOCAL)
    if (!state_->minivmac_bootstrap_attempted)
    {
        state_->minivmac_bootstrap_attempted = true;
        g_events->beginControlPanelNetworkTransfer();
        provision_minivmac_defaults();
        g_events->hideControlPanelDownload();
        g_events->endControlPanelNetworkTransfer();
    }
#endif
}

void ControlPanelService::tick(const WifiModeSnapshot &wifi)
{
    active_control_panel = this;
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
        if (!state_->minivmac_bootstrap_attempted)
        {
            state_->minivmac_bootstrap_attempted = true;
            if (g_events)
                g_events->beginControlPanelNetworkTransfer();
            provision_minivmac_defaults();
            if (g_events)
            {
                g_events->hideControlPanelDownload();
                g_events->endControlPanelNetworkTransfer();
            }
        }
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
