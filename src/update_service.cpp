#include "update_service.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <NetworkClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>

#include <strings.h>
#include <vector>

#include "maclock_version.h"

#ifndef MACLOCK_LOCAL
#include <NetworkClientSecure.h>
#include <SHA2Builder.h>
#include <Update.h>
#include <esp_app_desc.h>
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#include <miniz.h>

#include "github_ca.h"
#endif

namespace
{
static constexpr char kLatestReleaseUrl[] =
    "https://api.github.com/repos/fensoft/maclock/releases/latest";
static constexpr char kManifestName[] =
    "maclock-lolin-s3-update.json";
static constexpr char kFirmwareName[] =
    "maclock-lolin-s3-firmware.bin";
static constexpr char kAssetsName[] =
    "maclock-lolin-s3-assets.zip";
static constexpr char kDownloadedPrefix[] = "/downloaded/";
static constexpr char kAssetTemporaryPath[] =
    "/.maclock-asset.tmp";
static constexpr uint32_t kCheckIntervalMs =
    24UL * 60UL * 60UL * 1000UL;
static constexpr uint32_t kLaterIntervalMs =
    24UL * 60UL * 60UL * 1000UL;
static constexpr uint32_t kFirstBootValidationMs = 10000;
static constexpr uint32_t kNetworkTimeoutMs = 30000;
#ifndef MACLOCK_LOCAL
static constexpr size_t kAppSlotSize = 3U * 1024U * 1024U;
static constexpr size_t kFirmwarePrefixSize =
    sizeof(esp_image_header_t) +
    sizeof(esp_image_segment_header_t) +
    sizeof(esp_app_desc_t);
#endif

enum class WorkerAction : uint8_t
{
    None,
    Check,
    Install
};

struct SemanticVersion
{
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;
    bool valid = false;
};

SemanticVersion parse_version(const char *text)
{
    SemanticVersion result;
    if (!text)
        return result;
    if (*text == 'v' || *text == 'V')
        ++text;
    unsigned long major = 0;
    unsigned long minor = 0;
    unsigned long patch = 0;
    char tail = '\0';
    const int parsed = sscanf(
        text, "%lu.%lu.%lu%c",
        &major, &minor, &patch, &tail);
    result.valid = parsed == 3;
    if (result.valid)
    {
        result.major = static_cast<uint32_t>(major);
        result.minor = static_cast<uint32_t>(minor);
        result.patch = static_cast<uint32_t>(patch);
    }
    return result;
}

int compare_versions(const char *left, const char *right)
{
    const SemanticVersion a = parse_version(left);
    const SemanticVersion b = parse_version(right);
    if (!a.valid || !b.valid)
        return 0;
    if (a.major != b.major)
        return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor)
        return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch)
        return a.patch < b.patch ? -1 : 1;
    return 0;
}

String normalized_version(const char *version)
{
    if (!version)
        return {};
    return (*version == 'v' || *version == 'V')
               ? String(version + 1)
               : String(version);
}

bool protected_download_path(const char *path)
{
    return path &&
           (strcmp(path, "/downloaded") == 0 ||
            strncmp(
                path, kDownloadedPrefix,
                sizeof(kDownloadedPrefix) - 1) == 0);
}

bool valid_asset_path(const char *path)
{
    const size_t length = path ? strlen(path) : 0;
    if (!path || path[0] != '/' || path[1] == '\0' ||
        protected_download_path(path) ||
        strstr(path, "..") || strchr(path, '\\') ||
        strstr(path, "//") ||
        strcmp(path, kAssetTemporaryPath) == 0 ||
        length >= 192 || path[length - 1] == '/')
    {
        return false;
    }
    return true;
}

uint16_t read_u16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t read_u32(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

void copy_text(char *destination, size_t size, const String &text)
{
    if (!destination || !size)
        return;
    strlcpy(destination, text.c_str(), size);
}

void copy_text(
    char *destination, size_t size, const char *text)
{
    if (!destination || !size)
        return;
    strlcpy(destination, text ? text : "", size);
}

bool begin_http(
    HTTPClient &http, NetworkClient &client,
    const String &url)
{
    http.useHTTP10(true);
    http.setConnectTimeout(15000);
    http.setTimeout(kNetworkTimeoutMs);
    http.setUserAgent(
        String("Maclock/") + MACLOCK_VERSION +
        " (+https://github.com/fensoft/maclock)");
#ifndef MACLOCK_LOCAL
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
#endif
    return http.begin(client, url);
}

bool fetch_text(
    const String &url, String &payload, String &error)
{
#ifdef MACLOCK_LOCAL
    NetworkClient client;
#else
    NetworkClientSecure client;
    client.setCACert(kGithubRootCertificates);
    client.setHandshakeTimeout(30);
#endif
    HTTPClient http;
    if (!begin_http(http, client, url))
    {
        error = "Could not open the update server";
        return false;
    }
    const int response = http.GET();
    if (response != HTTP_CODE_OK)
    {
        if (response == 404)
            error = "No published Maclock release was found";
        else
            error = String("Update server returned HTTP ") +
                    String(response);
        http.end();
        return false;
    }
    payload = http.getString();
    http.end();
    if (!payload.length() || payload.length() > 1024U * 1024U)
    {
        error = "Update metadata is empty or too large";
        return false;
    }
    return true;
}

#ifndef MACLOCK_LOCAL
bool validate_firmware_prefix(
    const uint8_t *data, size_t length, String &error)
{
    if (!data || length < kFirmwarePrefixSize)
    {
        error = "The firmware header is incomplete";
        return false;
    }
    const auto *header =
        reinterpret_cast<const esp_image_header_t *>(data);
    if (header->magic != ESP_IMAGE_HEADER_MAGIC ||
        header->segment_count == 0 ||
        header->segment_count > ESP_IMAGE_MAX_SEGMENTS)
    {
        error = "The upload is not an ESP32 application image";
        return false;
    }
    if (header->chip_id != ESP_CHIP_ID_ESP32S3)
    {
        error = "The firmware is not built for ESP32-S3";
        return false;
    }
    const auto *description = reinterpret_cast<const esp_app_desc_t *>(
        data + sizeof(esp_image_header_t) +
        sizeof(esp_image_segment_header_t));
    if (description->magic_word != ESP_APP_DESC_MAGIC_WORD ||
        strncasecmp(
            description->project_name, "maclock", 7) != 0)
    {
        error = "The firmware is not a Maclock application";
        return false;
    }
    return true;
}

bool validate_ota_partitions(size_t firmware_size, String &error)
{
    const esp_partition_t *running =
        esp_ota_get_running_partition();
    const esp_partition_t *target =
        esp_ota_get_next_update_partition(nullptr);
    if (!running || !target ||
        running->size != kAppSlotSize ||
        target->size != kAppSlotSize ||
        firmware_size == 0 ||
        firmware_size > target->size)
    {
        error =
            "This device needs the Maclock 1.0 USB repartition first";
        return false;
    }
    return true;
}

bool equals_digest(const String &left, const char *right)
{
    return right && left.length() == 64 &&
           left.equalsIgnoreCase(right);
}

bool hash_file(const char *path, String &digest)
{
    fs::File file = LittleFS.open(path, "r");
    if (!file)
        return false;
    SHA256Builder hash;
    hash.begin();
    uint8_t buffer[4096];
    while (file.available())
    {
        const size_t count = file.read(buffer, sizeof(buffer));
        if (!count)
        {
            file.close();
            return false;
        }
        hash.add(buffer, count);
    }
    file.close();
    hash.calculate();
    digest = hash.toString();
    return true;
}

bool ensure_parent_directories(const char *path)
{
    if (!valid_asset_path(path))
        return false;
    char directory[192] = {};
    strlcpy(directory, path, sizeof(directory));
    for (char *cursor = directory + 1; *cursor; ++cursor)
    {
        if (*cursor != '/')
            continue;
        *cursor = '\0';
        if (!LittleFS.exists(directory) &&
            !LittleFS.mkdir(directory))
        {
            return false;
        }
        *cursor = '/';
    }
    return true;
}

JsonObjectConst find_manifest_file(
    JsonArrayConst files, const char *path)
{
    for (JsonObjectConst item : files)
    {
        const char *candidate = item["path"] | "";
        if (strcmp(candidate, path) == 0)
            return item;
    }
    return {};
}

bool manifest_contains(
    JsonArrayConst files, const char *path)
{
    return !find_manifest_file(files, path).isNull();
}

void collect_files(
    const char *path, std::vector<String> &files,
    std::vector<String> &directories)
{
    fs::File directory = LittleFS.open(path, "r");
    if (!directory || !directory.isDirectory())
    {
        directory.close();
        return;
    }
    fs::File entry = directory.openNextFile();
    while (entry)
    {
        const String entry_path = entry.path();
        const bool is_directory = entry.isDirectory();
        entry.close();
        if (protected_download_path(entry_path.c_str()))
        {
            entry = directory.openNextFile();
            continue;
        }
        if (is_directory)
        {
            collect_files(
                entry_path.c_str(), files, directories);
            directories.push_back(entry_path);
        }
        else if (entry_path != kAssetTemporaryPath)
        {
            files.push_back(entry_path);
        }
        entry = directory.openNextFile();
    }
    directory.close();
}

class HashedStream
{
public:
    HashedStream(NetworkClient &client, size_t length)
        : client_(client), remaining_(length)
    {
        hash_.begin();
    }

    bool readExact(uint8_t *destination, size_t length)
    {
        if (length > remaining_)
            return false;
        size_t read = 0;
        uint32_t idle_since = millis();
        while (read < length)
        {
            const int count = client_.read(
                destination + read, length - read);
            if (count > 0)
            {
                hash_.add(destination + read, count);
                read += static_cast<size_t>(count);
                remaining_ -= static_cast<size_t>(count);
                idle_since = millis();
                continue;
            }
            if (millis() - idle_since > kNetworkTimeoutMs)
                return false;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        return true;
    }

    bool discard(size_t length)
    {
        uint8_t buffer[1024];
        while (length)
        {
            const size_t count =
                min(length, sizeof(buffer));
            if (!readExact(buffer, count))
                return false;
            length -= count;
        }
        return true;
    }

    String finish()
    {
        hash_.calculate();
        return hash_.toString();
    }

    size_t remaining() const { return remaining_; }

private:
    NetworkClient &client_;
    size_t remaining_;
    SHA256Builder hash_;
};

bool copy_stored_entry(
    HashedStream &stream, fs::File &file,
    size_t compressed_size, SHA256Builder &hash,
    size_t &written)
{
    uint8_t buffer[4096];
    written = 0;
    while (compressed_size)
    {
        const size_t count =
            min(compressed_size, sizeof(buffer));
        if (!stream.readExact(buffer, count))
            return false;
        if (file.write(buffer, count) != count)
            return false;
        hash.add(buffer, count);
        written += count;
        compressed_size -= count;
    }
    return true;
}

bool inflate_entry(
    HashedStream &stream, fs::File &file,
    size_t compressed_size, SHA256Builder &hash,
    size_t &written)
{
    auto *dictionary =
        static_cast<uint8_t *>(malloc(TINFL_LZ_DICT_SIZE));
    if (!dictionary)
        return false;
    uint8_t input[4096];
    size_t input_offset = 0;
    size_t input_available = 0;
    size_t compressed_remaining = compressed_size;
    size_t output_offset = 0;
    written = 0;
    tinfl_decompressor decompressor;
    tinfl_init(&decompressor);
    tinfl_status status = TINFL_STATUS_NEEDS_MORE_INPUT;

    while (status != TINFL_STATUS_DONE)
    {
        if (!input_available)
        {
            if (!compressed_remaining)
            {
                free(dictionary);
                return false;
            }
            input_available =
                min(compressed_remaining, sizeof(input));
            if (!stream.readExact(input, input_available))
            {
                free(dictionary);
                return false;
            }
            compressed_remaining -= input_available;
            input_offset = 0;
        }

        size_t consumed = input_available;
        size_t produced = TINFL_LZ_DICT_SIZE - output_offset;
        const mz_uint32 flags =
            compressed_remaining ? TINFL_FLAG_HAS_MORE_INPUT : 0;
        status = tinfl_decompress(
            &decompressor, input + input_offset, &consumed,
            dictionary, dictionary + output_offset, &produced,
            flags);
        input_offset += consumed;
        input_available -= consumed;

        if (produced)
        {
            if (file.write(
                    dictionary + output_offset, produced) !=
                produced)
            {
                free(dictionary);
                return false;
            }
            hash.add(dictionary + output_offset, produced);
            written += produced;
            output_offset += produced;
            if (output_offset == TINFL_LZ_DICT_SIZE)
                output_offset = 0;
        }

        if (status < TINFL_STATUS_DONE ||
            (status == TINFL_STATUS_NEEDS_MORE_INPUT &&
             !input_available && !compressed_remaining))
        {
            free(dictionary);
            return false;
        }
    }

    const bool exact =
        !input_available && !compressed_remaining;
    free(dictionary);
    return exact;
}
#endif
} // namespace

struct UpdateService::State
{
    mutable portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    Preferences *preferences = nullptr;
    UpdateSnapshot snapshot;
    TaskHandle_t worker = nullptr;
    WorkerAction requested_action = WorkerAction::None;
    bool action_pending = false;
    bool check_requested = true;
    bool install_requested = false;
    bool first_check_complete = false;
    bool pending_verify = false;
    uint32_t validation_started_ms = 0;
    uint32_t last_check_ms = 0;
    uint32_t prompt_snoozed_until_ms = 0;
    String ignored_version;
    String release_etag;
    String manifest_url;
    String firmware_url;
    String assets_url;
#ifndef MACLOCK_LOCAL
    bool manual_upload = false;
    size_t manual_written = 0;
    bool manual_update_started = false;
    size_t manual_prefix_length = 0;
    uint8_t manual_prefix[kFirmwarePrefixSize] = {};
#endif
};

namespace
{
void set_stage(
    UpdateService::State &state, UpdateStage stage,
    const char *message, uint8_t progress = 0)
{
    portENTER_CRITICAL(&state.mux);
    state.snapshot.stage = stage;
    state.snapshot.busy =
        stage == UpdateStage::Checking ||
        stage == UpdateStage::DownloadingAssets ||
        stage == UpdateStage::InstallingAssets ||
        stage == UpdateStage::DownloadingFirmware ||
        stage == UpdateStage::UploadingFirmware;
    state.snapshot.progress = progress;
    copy_text(
        state.snapshot.message,
        sizeof(state.snapshot.message), message);
    portEXIT_CRITICAL(&state.mux);
}

void set_error(UpdateService::State &state, const String &error)
{
    portENTER_CRITICAL(&state.mux);
    state.snapshot.stage = UpdateStage::Error;
    state.snapshot.busy = false;
    state.snapshot.progress = 0;
    copy_text(
        state.snapshot.message,
        sizeof(state.snapshot.message), error);
    portEXIT_CRITICAL(&state.mux);
}

void set_progress(
    UpdateService::State &state, UpdateStage stage,
    uint8_t progress, const char *message)
{
    portENTER_CRITICAL(&state.mux);
    state.snapshot.stage = stage;
    state.snapshot.busy = true;
    state.snapshot.progress = progress;
    copy_text(
        state.snapshot.message,
        sizeof(state.snapshot.message), message);
    portEXIT_CRITICAL(&state.mux);
}

bool fetch_latest_release(
    UpdateService::State &state, String &payload,
    bool &not_modified, String &error)
{
#ifdef MACLOCK_LOCAL
    NetworkClient client;
#else
    NetworkClientSecure client;
    client.setCACert(kGithubRootCertificates);
    client.setHandshakeTimeout(30);
#endif
    HTTPClient http;
    if (!begin_http(http, client, kLatestReleaseUrl))
    {
        error = "Could not open the update server";
        return false;
    }
    const char *headers[] = {"ETag"};
    http.collectHeaders(headers, 1);
    if (state.release_etag.length() &&
        state.manifest_url.length())
    {
        http.addHeader("If-None-Match", state.release_etag);
    }
    const int response = http.GET();
    if (response == HTTP_CODE_NOT_MODIFIED)
    {
        not_modified = true;
        http.end();
        return true;
    }
    if (response != HTTP_CODE_OK)
    {
        error = response == 404
                    ? "No published Maclock release was found"
                    : String("Update server returned HTTP ") +
                          String(response);
        http.end();
        return false;
    }
    const String etag = http.header("ETag");
    payload = http.getString();
    http.end();
    if (!payload.length() || payload.length() > 1024U * 1024U)
    {
        error = "Update metadata is empty or too large";
        return false;
    }
    if (etag.length())
    {
        state.release_etag = etag;
        if (state.preferences)
            state.preferences->putString("otaEtag", etag);
    }
    return true;
}

bool perform_check(UpdateService::State &state)
{
    set_stage(state, UpdateStage::Checking, "Checking GitHub releases");
    String payload;
    String error;
    bool not_modified = false;
    if (!fetch_latest_release(
            state, payload, not_modified, error))
    {
        if (strncmp(
                error.c_str(), "No published", 12) == 0)
        {
            portENTER_CRITICAL(&state.mux);
            state.snapshot.stage = UpdateStage::UpToDate;
            state.snapshot.busy = false;
            state.snapshot.update_available = false;
            state.snapshot.prompt_pending = false;
            copy_text(
                state.snapshot.message,
                sizeof(state.snapshot.message), error);
            portEXIT_CRITICAL(&state.mux);
            return true;
        }
        set_error(state, error);
        return false;
    }
    if (not_modified)
    {
        portENTER_CRITICAL(&state.mux);
        state.snapshot.stage =
            state.snapshot.update_available
                ? UpdateStage::Available
                : UpdateStage::UpToDate;
        state.snapshot.busy = false;
        copy_text(
            state.snapshot.message,
            sizeof(state.snapshot.message),
            state.snapshot.update_available
                ? "A new Maclock release is available"
                : "Maclock is up to date");
        portEXIT_CRITICAL(&state.mux);
        return true;
    }

    JsonDocument document;
    const DeserializationError json_error =
        deserializeJson(document, payload);
    if (json_error)
    {
        set_error(state, "GitHub returned invalid release metadata");
        return false;
    }
    if (document["draft"] | false ||
        document["prerelease"] | false)
    {
        set_error(state, "The latest GitHub release is not stable");
        return false;
    }

    const String version =
        normalized_version(document["tag_name"] | "");
    if (!parse_version(version.c_str()).valid)
    {
        set_error(state, "The release tag is not a semantic version");
        return false;
    }

    String manifest_url;
    String firmware_url;
    String assets_url;
    for (JsonObjectConst asset :
         document["assets"].as<JsonArrayConst>())
    {
        const char *name = asset["name"] | "";
        const String url = asset["browser_download_url"] | "";
        if (strcmp(name, kManifestName) == 0)
            manifest_url = url;
        else if (strcmp(name, kFirmwareName) == 0)
            firmware_url = url;
        else if (strcmp(name, kAssetsName) == 0)
            assets_url = url;
    }
    if (!manifest_url.length() || !firmware_url.length() ||
        !assets_url.length())
    {
        set_error(state, "The release is missing update assets");
        return false;
    }

    const bool available =
        compare_versions(MACLOCK_VERSION, version.c_str()) < 0;
    portENTER_CRITICAL(&state.mux);
    state.manifest_url = manifest_url;
    state.firmware_url = firmware_url;
    state.assets_url = assets_url;
    copy_text(
        state.snapshot.latest_version,
        sizeof(state.snapshot.latest_version), version);
    copy_text(
        state.snapshot.release_url,
        sizeof(state.snapshot.release_url),
        document["html_url"] | "");
    copy_text(
        state.snapshot.release_notes,
        sizeof(state.snapshot.release_notes),
        document["body"] | "");
    state.snapshot.update_available = available;
    state.snapshot.prompt_pending =
        available && !version.equalsIgnoreCase(
                         state.ignored_version);
    state.snapshot.stage = available
                               ? UpdateStage::Available
                               : UpdateStage::UpToDate;
    state.snapshot.busy = false;
    state.snapshot.progress = 0;
    copy_text(
        state.snapshot.message,
        sizeof(state.snapshot.message),
        available ? "A new Maclock release is available"
                  : "Maclock is up to date");
    portEXIT_CRITICAL(&state.mux);
    return true;
}

#ifndef MACLOCK_LOCAL
bool validate_manifest(
    JsonDocument &manifest, const char *expected_version,
    String &error)
{
    const String version = manifest["version"] | "";
    if (!version.equalsIgnoreCase(expected_version))
    {
        error = "Release and update manifest versions differ";
        return false;
    }
    if (strcmp(
            manifest["board"] | "", MACLOCK_BOARD_ID) != 0 ||
        (manifest["partitionSchema"] | 0) !=
            MACLOCK_PARTITION_SCHEMA ||
        (manifest["assetSchema"] | 0) !=
            MACLOCK_ASSET_SCHEMA)
    {
        error = "This release is not compatible with this Maclock";
        return false;
    }
    JsonObjectConst firmware = manifest["firmware"];
    JsonObjectConst assets = manifest["assets"];
    if (strcmp(firmware["name"] | "", kFirmwareName) != 0 ||
        strcmp(assets["name"] | "", kAssetsName) != 0 ||
        (firmware["size"] | 0U) == 0 ||
        (assets["size"] | 0U) == 0 ||
        strlen(firmware["sha256"] | "") != 64 ||
        strlen(assets["sha256"] | "") != 64 ||
        !assets["files"].is<JsonArrayConst>())
    {
        error = "The update manifest is incomplete";
        return false;
    }
    std::vector<String> manifest_paths;
    for (JsonObjectConst file :
         assets["files"].as<JsonArrayConst>())
    {
        const char *path = file["path"] | "";
        if (!valid_asset_path(path) ||
            strlen(file["sha256"] | "") != 64 ||
            (file["method"] | 99) > 8 ||
            ((file["method"] | 99) != 0 &&
             (file["method"] | 99) != 8))
        {
            error = "The update manifest contains an unsafe asset";
            return false;
        }
        for (const String &other : manifest_paths)
        {
            if (other == path)
            {
                error =
                    "The update manifest contains duplicate assets";
                return false;
            }
        }
        manifest_paths.emplace_back(path);
    }
    return true;
}

bool install_assets(
    UpdateService::State &state,
    JsonObjectConst assets, String &error)
{
    JsonArrayConst files = assets["files"].as<JsonArrayConst>();
    size_t largest_temporary_file = 0;
    uint16_t changed_before_download = 0;
    for (JsonObjectConst file : files)
    {
        const char *path = file["path"] | "";
        String installed_digest;
        const bool unchanged =
            LittleFS.exists(path) &&
            hash_file(path, installed_digest) &&
            equals_digest(
                installed_digest, file["sha256"] | "");
        if (!unchanged)
        {
            ++changed_before_download;
            const size_t size = file["size"] | 0U;
            if (size > largest_temporary_file)
                largest_temporary_file = size;
        }
    }
    const size_t free_bytes =
        LittleFS.totalBytes() - LittleFS.usedBytes();
    if (largest_temporary_file > free_bytes)
    {
        error =
            "Not enough LittleFS space. Remove downloaded sounds "
            "through Sound Manager and try again.";
        return false;
    }
    portENTER_CRITICAL(&state.mux);
    state.snapshot.changed_assets = changed_before_download;
    portEXIT_CRITICAL(&state.mux);

    NetworkClientSecure client;
    client.setCACert(kGithubRootCertificates);
    client.setHandshakeTimeout(30);
    HTTPClient http;
    if (!begin_http(http, client, state.assets_url))
    {
        error = "Could not open the asset download";
        return false;
    }
    const int response = http.GET();
    if (response != HTTP_CODE_OK)
    {
        error = String("Asset download returned HTTP ") +
                String(response);
        http.end();
        return false;
    }
    const int content_length = http.getSize();
    const size_t expected_length = assets["size"] | 0U;
    if (content_length <= 0 ||
        static_cast<size_t>(content_length) != expected_length)
    {
        error = "The asset ZIP size does not match its manifest";
        http.end();
        return false;
    }
    NetworkClient *stream = http.getStreamPtr();
    if (!stream)
    {
        error = "The asset ZIP stream is unavailable";
        http.end();
        return false;
    }

    HashedStream input(*stream, expected_length);
    size_t processed = 0;
    uint16_t changed = 0;
    bool central_directory = false;
    std::vector<String> archive_paths;
    LittleFS.remove(kAssetTemporaryPath);

    while (input.remaining() >= 4)
    {
        uint8_t signature_bytes[4];
        if (!input.readExact(signature_bytes, sizeof(signature_bytes)))
        {
            error = "The asset ZIP ended unexpectedly";
            break;
        }
        const uint32_t signature = read_u32(signature_bytes);
        if (signature == 0x02014b50UL)
        {
            uint32_t central_signature = signature;
            size_t central_entries = 0;
            while (central_signature == 0x02014b50UL)
            {
                uint8_t central[42];
                if (!input.readExact(
                        central, sizeof(central)))
                {
                    error =
                        "The ZIP central directory was truncated";
                    break;
                }
                const uint16_t made_by =
                    read_u16(central);
                const uint16_t flags =
                    read_u16(central + 4);
                const uint16_t method =
                    read_u16(central + 6);
                const uint32_t compressed_size =
                    read_u32(central + 16);
                const uint32_t uncompressed_size =
                    read_u32(central + 20);
                const uint16_t name_length =
                    read_u16(central + 24);
                const uint16_t extra_length =
                    read_u16(central + 26);
                const uint16_t comment_length =
                    read_u16(central + 28);
                const uint32_t attributes =
                    read_u32(central + 34);
                if ((flags & 0x0009U) ||
                    (method != 0 && method != 8) ||
                    name_length == 0 ||
                    name_length >= 191 ||
                    extra_length > 4096 ||
                    comment_length > 4096 ||
                    compressed_size == UINT32_MAX ||
                    uncompressed_size == UINT32_MAX ||
                    (((made_by >> 8) == 3) &&
                     (((attributes >> 16) & 0xF000U) ==
                      0xA000U)))
                {
                    error =
                        "The ZIP central directory is unsafe";
                    break;
                }
                char central_name[192] = {};
                if (!input.readExact(
                        reinterpret_cast<uint8_t *>(
                            central_name),
                        name_length) ||
                    !input.discard(
                        extra_length + comment_length))
                {
                    error =
                        "The ZIP central directory was truncated";
                    break;
                }
                central_name[name_length] = '\0';
                String central_path =
                    central_name[0] == '/'
                        ? String(central_name)
                        : String("/") + central_name;
                JsonObjectConst central_file =
                    find_manifest_file(
                        files, central_path.c_str());
                if (central_file.isNull() ||
                    !valid_asset_path(
                        central_path.c_str()) ||
                    static_cast<uint16_t>(
                        central_file["method"] | 99) !=
                        method ||
                    static_cast<uint32_t>(
                        central_file["compressedSize"] |
                        UINT32_MAX) != compressed_size ||
                    static_cast<uint32_t>(
                        central_file["size"] | UINT32_MAX) !=
                        uncompressed_size)
                {
                    error =
                        "The ZIP central directory and manifest differ";
                    break;
                }
                ++central_entries;
                if (!input.readExact(
                        signature_bytes,
                        sizeof(signature_bytes)))
                {
                    error =
                        "The ZIP central directory was truncated";
                    break;
                }
                central_signature =
                    read_u32(signature_bytes);
            }
            if (!error.length() &&
                central_signature != 0x06054b50UL)
            {
                error =
                    "The ZIP end record is missing";
            }
            if (!error.length() &&
                !input.discard(input.remaining()))
            {
                error =
                    "The ZIP end record was truncated";
            }
            central_directory =
                !error.length() &&
                central_entries == files.size();
            break;
        }
        if (signature == 0x06054b50UL)
        {
            error = "The ZIP contains no central directory";
            break;
        }
        if (signature != 0x04034b50UL)
        {
            error = "The asset ZIP contains an invalid record";
            break;
        }

        uint8_t header[26];
        if (!input.readExact(header, sizeof(header)))
        {
            error = "The asset ZIP header was truncated";
            break;
        }
        const uint16_t flags = read_u16(header + 2);
        const uint16_t method = read_u16(header + 4);
        const uint32_t compressed_size = read_u32(header + 14);
        const uint32_t uncompressed_size = read_u32(header + 18);
        const uint16_t name_length = read_u16(header + 22);
        const uint16_t extra_length = read_u16(header + 24);
        if (read_u16(header) > 20 ||
            (flags & 0x0009U) || name_length == 0 ||
            name_length >= 191 || extra_length > 4096 ||
            compressed_size == UINT32_MAX ||
            uncompressed_size == UINT32_MAX)
        {
            error = "The asset ZIP uses an unsupported feature";
            break;
        }

        char archive_name[192] = {};
        if (!input.readExact(
                reinterpret_cast<uint8_t *>(archive_name),
                name_length) ||
            !input.discard(extra_length))
        {
            error = "The asset ZIP filename was truncated";
            break;
        }
        archive_name[name_length] = '\0';
        String path = archive_name[0] == '/'
                          ? String(archive_name)
                          : String("/") + archive_name;
        JsonObjectConst file =
            find_manifest_file(files, path.c_str());
        if (file.isNull() || !valid_asset_path(path.c_str()) ||
            static_cast<uint16_t>(file["method"] | 99) != method ||
            static_cast<uint32_t>(
                file["compressedSize"] | UINT32_MAX) !=
                compressed_size ||
            static_cast<uint32_t>(
                file["size"] | UINT32_MAX) !=
                uncompressed_size)
        {
            error = "The ZIP and asset manifest do not match";
            break;
        }
        for (const String &seen : archive_paths)
        {
            if (seen == path)
            {
                error = "The ZIP contains a duplicate asset";
                break;
            }
        }
        if (error.length())
            break;
        archive_paths.push_back(path);

        String installed_digest;
        const bool unchanged =
            LittleFS.exists(path.c_str()) &&
            hash_file(path.c_str(), installed_digest) &&
            equals_digest(
                installed_digest, file["sha256"] | "");
        if (unchanged)
        {
            if (!input.discard(compressed_size))
            {
                error = "An unchanged ZIP entry was truncated";
                break;
            }
        }
        else
        {
            ++changed;
            if (!ensure_parent_directories(path.c_str()))
            {
                error = "Could not create an asset directory";
                break;
            }
            LittleFS.remove(kAssetTemporaryPath);
            fs::File output =
                LittleFS.open(kAssetTemporaryPath, "w");
            if (!output)
            {
                error =
                    "LittleFS has insufficient space for the update";
                break;
            }
            SHA256Builder hash;
            hash.begin();
            size_t written = 0;
            const bool copied =
                method == 0
                    ? copy_stored_entry(
                          input, output, compressed_size,
                          hash, written)
                    : inflate_entry(
                          input, output, compressed_size,
                          hash, written);
            output.close();
            hash.calculate();
            const String digest = hash.toString();
            if (!copied || written != uncompressed_size ||
                !equals_digest(digest, file["sha256"] | ""))
            {
                LittleFS.remove(kAssetTemporaryPath);
                error =
                    "An asset failed decompression or verification";
                break;
            }
            if (!LittleFS.rename(
                    kAssetTemporaryPath, path.c_str()))
            {
                LittleFS.remove(kAssetTemporaryPath);
                error = "Could not install a verified asset";
                break;
            }
        }

        ++processed;
        const uint8_t progress =
            files.size()
                ? static_cast<uint8_t>(
                      min<size_t>(
                          99, processed * 100 / files.size()))
                : 99;
        set_progress(
            state, UpdateStage::InstallingAssets,
            progress, "Installing LittleFS assets");
    }

    const String zip_digest = input.finish();
    http.end();
    LittleFS.remove(kAssetTemporaryPath);
    if (error.length())
        return false;
    if (!central_directory || processed != files.size() ||
        !equals_digest(zip_digest, assets["sha256"] | ""))
    {
        error = "The complete asset ZIP failed verification";
        return false;
    }

    std::vector<String> installed;
    std::vector<String> directories;
    collect_files("/", installed, directories);
    for (const String &path : installed)
    {
        if (!manifest_contains(files, path.c_str()) &&
            !protected_download_path(path.c_str()))
        {
            LittleFS.remove(path.c_str());
        }
    }
    for (auto item = directories.rbegin();
         item != directories.rend(); ++item)
    {
        if (!protected_download_path(item->c_str()))
            LittleFS.rmdir(item->c_str());
    }

    portENTER_CRITICAL(&state.mux);
    state.snapshot.changed_assets = changed;
    portEXIT_CRITICAL(&state.mux);
    return true;
}

bool install_firmware(
    UpdateService::State &state,
    JsonObjectConst firmware, String &error)
{
    const size_t expected_size = firmware["size"] | 0U;
    if (!validate_ota_partitions(expected_size, error))
        return false;
    NetworkClientSecure client;
    client.setCACert(kGithubRootCertificates);
    client.setHandshakeTimeout(30);
    HTTPClient http;
    if (!begin_http(http, client, state.firmware_url))
    {
        error = "Could not open the firmware download";
        return false;
    }
    const int response = http.GET();
    if (response != HTTP_CODE_OK ||
        http.getSize() != static_cast<int>(expected_size))
    {
        error = "The firmware download size is invalid";
        http.end();
        return false;
    }
    NetworkClient *stream = http.getStreamPtr();
    if (!stream)
    {
        error = "The firmware download stream is unavailable";
        http.end();
        return false;
    }

    SHA256Builder hash;
    hash.begin();
    uint8_t buffer[4096];
    size_t received = 0;
    uint32_t idle_since = millis();
    while (received < kFirmwarePrefixSize)
    {
        const int count = stream->read(
            buffer + received, kFirmwarePrefixSize - received);
        if (count > 0)
        {
            received += static_cast<size_t>(count);
            idle_since = millis();
            continue;
        }
        if (millis() - idle_since > kNetworkTimeoutMs)
        {
            http.end();
            error = "The firmware header download timed out";
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (!validate_firmware_prefix(buffer, received, error) ||
        !Update.begin(expected_size, U_FLASH))
    {
        if (!error.length())
            error = "The inactive firmware partition is unavailable";
        http.end();
        return false;
    }
    hash.add(buffer, received);
    if (Update.write(buffer, received) != received)
    {
        Update.abort();
        http.end();
        error = String("Firmware write failed: ") +
                Update.errorString();
        return false;
    }
    while (received < expected_size)
    {
        const size_t wanted =
            min(expected_size - received, sizeof(buffer));
        const int count = stream->read(buffer, wanted);
        if (count <= 0)
        {
            if (millis() - idle_since <= kNetworkTimeoutMs)
            {
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }
            Update.abort();
            http.end();
            error = "The firmware download timed out";
            return false;
        }
        idle_since = millis();
        hash.add(buffer, static_cast<size_t>(count));
        if (Update.write(buffer, static_cast<size_t>(count)) !=
            static_cast<size_t>(count))
        {
            Update.abort();
            http.end();
            error = String("Firmware write failed: ") +
                    Update.errorString();
            return false;
        }
        received += static_cast<size_t>(count);
        set_progress(
            state, UpdateStage::DownloadingFirmware,
            static_cast<uint8_t>(
                min<size_t>(
                    99, received * 100 / expected_size)),
            "Installing firmware");
    }
    http.end();
    hash.calculate();
    if (!equals_digest(
            hash.toString(), firmware["sha256"] | ""))
    {
        Update.abort();
        error = "The firmware SHA-256 does not match";
        return false;
    }
    if (!Update.end(false))
    {
        error = String("Firmware validation failed: ") +
                Update.errorString();
        return false;
    }
    return true;
}

bool perform_install(UpdateService::State &state)
{
    set_stage(
        state, UpdateStage::DownloadingAssets,
        "Reading the release manifest");
    String payload;
    String error;
    if (!fetch_text(state.manifest_url, payload, error))
    {
        set_error(state, error);
        return false;
    }
    JsonDocument manifest;
    if (deserializeJson(manifest, payload))
    {
        set_error(state, "The update manifest is invalid");
        return false;
    }
    char target_version[32] = {};
    portENTER_CRITICAL(&state.mux);
    strlcpy(
        target_version, state.snapshot.latest_version,
        sizeof(target_version));
    portEXIT_CRITICAL(&state.mux);
    if (!validate_manifest(manifest, target_version, error))
    {
        set_error(state, error);
        return false;
    }
    if (state.preferences)
    {
        state.preferences->putBool("assetWork", true);
        state.preferences->putString(
            "assetTarget", target_version);
    }
    if (!install_assets(
            state, manifest["assets"].as<JsonObjectConst>(),
            error))
    {
        set_error(state, error);
        return false;
    }
    if (state.preferences)
    {
        state.preferences->putBool("assetWork", false);
        state.preferences->remove("assetTarget");
    }
    set_progress(
        state, UpdateStage::DownloadingFirmware, 0,
        "Downloading firmware");
    if (!install_firmware(
            state, manifest["firmware"].as<JsonObjectConst>(),
            error))
    {
        set_error(state, error);
        return false;
    }

    if (state.preferences)
    {
        state.preferences->putString("assetVer", target_version);
        state.preferences->putBool("otaPending", true);
    }
    portENTER_CRITICAL(&state.mux);
    copy_text(
        state.snapshot.asset_version,
        sizeof(state.snapshot.asset_version), target_version);
    state.snapshot.stage = UpdateStage::ReadyToReboot;
    state.snapshot.busy = false;
    state.snapshot.progress = 100;
    state.snapshot.reboot_required = true;
    copy_text(
        state.snapshot.message,
        sizeof(state.snapshot.message),
        "Update installed; reboot to finish");
    portEXIT_CRITICAL(&state.mux);
    return true;
}
#endif

void update_worker(void *context)
{
    auto *state =
        static_cast<UpdateService::State *>(context);
    WorkerAction action = WorkerAction::None;
    portENTER_CRITICAL(&state->mux);
    action = state->requested_action;
    state->requested_action = WorkerAction::None;
    portEXIT_CRITICAL(&state->mux);

    if (action == WorkerAction::Check)
        perform_check(*state);
    else if (action == WorkerAction::Install)
    {
#ifdef MACLOCK_LOCAL
        set_error(
            *state,
            "Firmware installation is unavailable in the simulator");
#else
        perform_install(*state);
#endif
    }

    portENTER_CRITICAL(&state->mux);
    state->worker = nullptr;
    portEXIT_CRITICAL(&state->mux);
    vTaskDelete(nullptr);
}

bool start_worker(
    UpdateService::State &state, WorkerAction action)
{
    portENTER_CRITICAL(&state.mux);
    if (state.worker || state.snapshot.busy)
    {
        portEXIT_CRITICAL(&state.mux);
        return false;
    }
    state.requested_action = action;
    const BaseType_t created = xTaskCreatePinnedToCore(
        update_worker, "MaclockUpdate", 32768, &state,
        1, &state.worker, 0);
    if (created != pdPASS)
        state.worker = nullptr;
    portEXIT_CRITICAL(&state.mux);
    return created == pdPASS;
}
} // namespace

void UpdateService::begin(Preferences &preferences)
{
    if (!state_)
        state_ = new State();
    state_->preferences = &preferences;
    copy_text(
        state_->snapshot.current_version,
        sizeof(state_->snapshot.current_version),
        MACLOCK_VERSION);
    const String asset_version =
        preferences.getString("assetVer", MACLOCK_VERSION);
    copy_text(
        state_->snapshot.asset_version,
        sizeof(state_->snapshot.asset_version),
        asset_version);
    state_->ignored_version =
        preferences.getString("otaIgnored", "");
    state_->release_etag =
        preferences.getString("otaEtag", "");
    state_->install_requested =
        preferences.getBool("assetWork", false);
    if (state_->install_requested)
    {
        state_->snapshot.stage = UpdateStage::InstallingAssets;
        copy_text(
            state_->snapshot.message,
            sizeof(state_->snapshot.message),
            "Interrupted update will resume after Wi-Fi connects");
    }
#ifdef MACLOCK_LOCAL
    state_->snapshot.supported = false;
    state_->snapshot.stage = UpdateStage::Idle;
#else
    const esp_partition_t *running =
        esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;
    if (running &&
        esp_ota_get_state_partition(running, &ota_state) ==
            ESP_OK &&
        ota_state == ESP_OTA_IMG_PENDING_VERIFY)
    {
        state_->pending_verify = true;
        state_->validation_started_ms = millis();
    }
#endif
}

void UpdateService::tick(
    const WifiModeSnapshot &wifi, bool allow_device_prompt)
{
    if (!state_)
        return;
#ifndef MACLOCK_LOCAL
    if (state_->pending_verify &&
        millis() - state_->validation_started_ms >=
            kFirstBootValidationMs)
    {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
        {
            state_->pending_verify = false;
            if (state_->preferences)
                state_->preferences->putBool(
                    "otaPending", false);
        }
    }
#endif

    const uint32_t now = millis();
    if (wifi.enabled && wifi.connected && !wifi.portal_active &&
        (state_->check_requested ||
         (!state_->first_check_complete ||
          now - state_->last_check_ms >= kCheckIntervalMs)))
    {
        if (start_worker(*state_, WorkerAction::Check))
        {
            state_->check_requested = false;
            state_->first_check_complete = true;
            state_->last_check_ms = now;
        }
    }
    if (state_->install_requested &&
        !state_->snapshot.busy &&
        state_->snapshot.update_available &&
        state_->manifest_url.length() &&
        state_->firmware_url.length() &&
        state_->assets_url.length())
    {
        if (start_worker(*state_, WorkerAction::Install))
            state_->install_requested = false;
    }

    portENTER_CRITICAL(&state_->mux);
    if (!allow_device_prompt ||
        static_cast<int32_t>(
            state_->prompt_snoozed_until_ms - now) > 0)
    {
        state_->snapshot.prompt_pending = false;
    }
    else if (state_->snapshot.update_available &&
             !String(state_->snapshot.latest_version)
                  .equalsIgnoreCase(state_->ignored_version))
    {
        state_->snapshot.prompt_pending = true;
    }
    portEXIT_CRITICAL(&state_->mux);
}

UpdateSnapshot UpdateService::snapshot() const
{
    UpdateSnapshot result;
    if (!state_)
        return result;
    portENTER_CRITICAL(&state_->mux);
    result = state_->snapshot;
    portEXIT_CRITICAL(&state_->mux);
    return result;
}

bool UpdateService::requestCheck()
{
    if (!state_)
        return false;
    state_->check_requested = true;
    return true;
}

bool UpdateService::requestInstall()
{
    if (!state_)
        return false;
    portENTER_CRITICAL(&state_->mux);
    const bool available =
        state_->snapshot.update_available &&
        state_->manifest_url.length() &&
        state_->firmware_url.length() &&
        state_->assets_url.length();
    portEXIT_CRITICAL(&state_->mux);
    return available &&
           start_worker(*state_, WorkerAction::Install);
}

void UpdateService::dismiss(bool ignore_version)
{
    if (!state_)
        return;
    portENTER_CRITICAL(&state_->mux);
    state_->snapshot.prompt_pending = false;
    if (ignore_version)
    {
        state_->ignored_version =
            state_->snapshot.latest_version;
        if (state_->preferences)
            state_->preferences->putString(
                "otaIgnored", state_->ignored_version);
    }
    else
    {
        state_->prompt_snoozed_until_ms =
            millis() + kLaterIntervalMs;
    }
    portEXIT_CRITICAL(&state_->mux);
}

bool UpdateService::consumePrompt()
{
    if (!state_)
        return false;
    portENTER_CRITICAL(&state_->mux);
    const bool pending = state_->snapshot.prompt_pending;
    state_->snapshot.prompt_pending = false;
    portEXIT_CRITICAL(&state_->mux);
    return pending;
}

bool UpdateService::beginManualFirmware(const char *filename)
{
    const size_t filename_length =
        filename ? strlen(filename) : 0;
    if (!state_ || filename_length < 4 ||
        strcasecmp(
            filename + filename_length - 4, ".bin") != 0)
    {
        return false;
    }
#ifdef MACLOCK_LOCAL
    set_error(
        *state_,
        "Firmware upload is unavailable in the simulator");
    return false;
#else
    portENTER_CRITICAL(&state_->mux);
    if (state_->snapshot.busy || state_->manual_upload)
    {
        portEXIT_CRITICAL(&state_->mux);
        return false;
    }
    state_->manual_upload = true;
    state_->manual_written = 0;
    state_->manual_update_started = false;
    state_->manual_prefix_length = 0;
    portEXIT_CRITICAL(&state_->mux);
    String partition_error;
    if (!validate_ota_partitions(1, partition_error))
    {
        state_->manual_upload = false;
        set_error(*state_, partition_error);
        return false;
    }
    set_stage(
        *state_, UpdateStage::UploadingFirmware,
        "Uploading firmware");
    return true;
#endif
}

bool UpdateService::writeManualFirmware(
    const uint8_t *data, size_t length)
{
#ifdef MACLOCK_LOCAL
    (void)data;
    (void)length;
    return false;
#else
    if (!state_ || !state_->manual_upload ||
        !data || !length)
    {
        return false;
    }
    if (!state_->manual_update_started)
    {
        const size_t required =
            kFirmwarePrefixSize - state_->manual_prefix_length;
        const size_t copied = length < required
                                  ? length
                                  : required;
        memcpy(
            state_->manual_prefix +
                state_->manual_prefix_length,
            data, copied);
        state_->manual_prefix_length += copied;
        data += copied;
        length -= copied;
        if (state_->manual_prefix_length <
            kFirmwarePrefixSize)
        {
            return true;
        }

        String error;
        if (!validate_firmware_prefix(
                state_->manual_prefix,
                state_->manual_prefix_length, error) ||
            !Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
        {
            state_->manual_upload = false;
            set_error(
                *state_,
                error.length() ? error : Update.errorString());
            return false;
        }
        state_->manual_update_started = true;
        const size_t prefix_written = Update.write(
            state_->manual_prefix,
            state_->manual_prefix_length);
        state_->manual_written = prefix_written;
        if (prefix_written != state_->manual_prefix_length)
            return false;
    }
    if (!length)
        return true;
    const size_t written =
        Update.write(const_cast<uint8_t *>(data), length);
    state_->manual_written += written;
    return written == length &&
           state_->manual_written <= kAppSlotSize;
#endif
}

bool UpdateService::finishManualFirmware()
{
#ifdef MACLOCK_LOCAL
    return false;
#else
    if (!state_ || !state_->manual_upload ||
        !state_->manual_update_started ||
        !state_->manual_written)
    {
        return false;
    }
    const bool finished = Update.end(true);
    state_->manual_upload = false;
    if (!finished)
    {
        set_error(*state_, Update.errorString());
        return false;
    }
    portENTER_CRITICAL(&state_->mux);
    state_->snapshot.stage = UpdateStage::ReadyToReboot;
    state_->snapshot.busy = false;
    state_->snapshot.progress = 100;
    state_->snapshot.reboot_required = true;
    copy_text(
        state_->snapshot.message,
        sizeof(state_->snapshot.message),
        "Firmware uploaded; reboot to finish");
    portEXIT_CRITICAL(&state_->mux);
    return true;
#endif
}

void UpdateService::abortManualFirmware()
{
#ifndef MACLOCK_LOCAL
    if (!state_ || !state_->manual_upload)
        return;
    if (state_->manual_update_started)
        Update.abort();
    state_->manual_upload = false;
    set_error(*state_, "Firmware upload was cancelled");
#endif
}

bool UpdateService::reboot()
{
    if (!state_)
        return false;
#ifdef MACLOCK_LOCAL
    return false;
#else
    if (!snapshot().reboot_required)
        return false;
    delay(100);
    ESP.restart();
    return true;
#endif
}
