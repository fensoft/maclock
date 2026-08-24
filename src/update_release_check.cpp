#ifdef MACLOCK_UPDATE_COMBINED_SOURCE
#include "update_service.h"
#include "update_service_internal.h"

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
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <mbedtls/platform.h>
#include <miniz.h>
#endif

namespace
{
#ifndef MACLOCK_LOCAL
class GithubNetworkClientSecure final
    : public NetworkClientSecure
{
public:
    void useSystemCertificateBundle()
    {
        attach_ssl_certificate_bundle(sslclient.get(), true);
        _use_ca_bundle = true;
    }
};
#endif

static constexpr char kLatestReleaseUrl[] =
    "https://api.github.com/repos/fensoft/maclock/releases/latest";
static constexpr char kManifestName[] =
    "maclock-lolin-s3-update.json";
static constexpr char kFirmwareName[] =
    "maclock-lolin-s3-firmware.bin";
static constexpr char kAssetsName[] =
    "maclock-lolin-s3-assets.zip";
static constexpr char kDownloadedPrefix[] = "/downloaded/";
static constexpr char kScreensaverPrefix[] = "/screensaver/";
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

#ifndef MACLOCK_LOCAL
void *tls_psram_calloc(size_t count, size_t size)
{
    void *memory = heap_caps_calloc(
        count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!memory)
        memory = heap_caps_calloc(
            count, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return memory;
}

void tls_psram_free(void *memory)
{
    heap_caps_free(memory);
}
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

bool protected_user_path(const char *path)
{
    if (!path)
        return false;
    if (strcmp(path, "/downloaded") == 0 ||
        strncmp(
            path, kDownloadedPrefix,
            sizeof(kDownloadedPrefix) - 1) == 0 ||
        strcmp(path, "/screensaver") == 0 ||
        strncmp(
            path, kScreensaverPrefix,
            sizeof(kScreensaverPrefix) - 1) == 0 ||
        strcmp(path, "/vMac.ROM") == 0)
    {
        return true;
    }

    // Uploaded sounds, slideshow photos, and Mini vMac media are
    // user-managed and must survive release-asset reconciliation.
    return strlen(path) == strlen("/disk1.dsk") &&
           strncmp(path, "/disk", strlen("/disk")) == 0 &&
           path[5] >= '1' && path[5] <= '6' &&
           strcmp(path + 6, ".dsk") == 0;
}

bool valid_asset_path(const char *path)
{
    const size_t length = path ? strlen(path) : 0;
    if (!path || path[0] != '/' || path[1] == '\0' ||
        protected_user_path(path) ||
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
    const String &url, bool follow_redirects = true)
{
    http.useHTTP10(true);
    http.setConnectTimeout(15000);
    http.setTimeout(kNetworkTimeoutMs);
    http.setUserAgent(
        String("Maclock/") + MACLOCK_VERSION +
        " (+https://github.com/fensoft/maclock)");
#ifndef MACLOCK_LOCAL
    http.setFollowRedirects(
        follow_redirects
            ? HTTPC_STRICT_FOLLOW_REDIRECTS
            : HTTPC_DISABLE_FOLLOW_REDIRECTS);
#else
    (void)follow_redirects;
#endif
    return http.begin(client, url);
}

#ifndef MACLOCK_LOCAL
bool is_http_redirect(int response)
{
    return response == HTTP_CODE_MOVED_PERMANENTLY ||
           response == HTTP_CODE_FOUND ||
           response == HTTP_CODE_SEE_OTHER ||
           response == HTTP_CODE_TEMPORARY_REDIRECT ||
           response == HTTP_CODE_PERMANENT_REDIRECT;
}

String resolve_redirect_url(
    const String &current_url, const String &location)
{
    if (location.startsWith("https://") ||
        location.startsWith("http://"))
    {
        return location;
    }
    if (!location.startsWith("/"))
        return String();
    const int scheme_end = current_url.indexOf("://");
    if (scheme_end < 0)
        return String();
    const int path_start =
        current_url.indexOf('/', scheme_end + 3);
    if (path_start < 0)
        return current_url + location;
    return current_url.substring(0, path_start) + location;
}

void set_http_connection_error(
    String &error, const char *prefix, int response,
    GithubNetworkClientSecure &client)
{
    error = String(prefix) + ": " +
            HTTPClient::errorToString(response);
    char tls_error[160] = {};
    client.lastError(tls_error, sizeof(tls_error));
    if (tls_error[0] && strcmp(tls_error, "UNKNOWN ERROR CODE") != 0)
        error += String(" (") + tls_error + ")";
}

bool resolve_download_url(
    const String &source_url, String &download_url,
    String &error)
{
    String current_url = source_url;
    for (uint8_t redirect = 0; redirect < 5; ++redirect)
    {
        GithubNetworkClientSecure client;
        client.useSystemCertificateBundle();
        client.setHandshakeTimeout(30);
        HTTPClient http;
        if (!begin_http(http, client, current_url, false))
        {
            error = "Could not open the release download";
            return false;
        }
        const int response = http.sendRequest("HEAD");
        if (is_http_redirect(response))
        {
            const String next_url = resolve_redirect_url(
                current_url, http.getLocation());
            http.end();
            client.stop();
            if (!next_url.length())
            {
                error = "The release returned an invalid redirect";
                return false;
            }
            current_url = next_url;
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        if (response != HTTP_CODE_OK)
        {
            if (response < 0)
            {
                set_http_connection_error(
                    error, "Release download connection failed",
                    response, client);
            }
            else
            {
                error = String("Release download returned HTTP ") +
                        String(response);
            }
            http.end();
            return false;
        }
        http.end();
        client.stop();
        download_url = current_url;
        return true;
    }
    error = "The release download redirected too many times";
    return false;
}
#endif

bool fetch_text(
    const String &url, String &payload, String &error)
{
#ifdef MACLOCK_LOCAL
    NetworkClient client;
    HTTPClient http;
    if (!begin_http(http, client, url, false))
    {
        error = "Could not open the update server";
        return false;
    }
    const int response = http.GET();
    if (response != HTTP_CODE_OK)
    {
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
#else
    String current_url = url;
    for (uint8_t redirect = 0; redirect < 5; ++redirect)
    {
        GithubNetworkClientSecure client;
        client.useSystemCertificateBundle();
        client.setHandshakeTimeout(30);
        HTTPClient http;
        if (!begin_http(http, client, current_url, false))
        {
            error = "Could not open the update server";
            return false;
        }
        const int response = http.GET();
        if (is_http_redirect(response))
        {
            const String next_url = resolve_redirect_url(
                current_url, http.getLocation());
            http.end();
            client.stop();
            if (!next_url.length())
            {
                error = "The update server returned an invalid redirect";
                return false;
            }
            current_url = next_url;
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        if (response != HTTP_CODE_OK)
        {
            if (response == 404)
            {
                error = "No published Maclock release was found";
            }
            else if (response < 0)
            {
                set_http_connection_error(
                    error, "Update connection failed",
                    response, client);
            }
            else
            {
                error = String("Update server returned HTTP ") +
                        String(response);
            }
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
    error = "The update server redirected too many times";
    return false;
#endif
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

__attribute__((noinline))
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
        if (protected_user_path(entry_path.c_str()))
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
    using ProgressCallback =
        void (*)(void *context, uint8_t progress);

    HashedStream(
        NetworkClient &client, size_t length,
        ProgressCallback progress_callback = nullptr,
        void *progress_context = nullptr)
        : client_(client), remaining_(length), total_(length),
          progress_callback_(progress_callback),
          progress_context_(progress_context)
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
                reportProgress();
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
    void reportProgress()
    {
        if (!progress_callback_ || !total_)
            return;
        const uint8_t progress = static_cast<uint8_t>(
            min<size_t>(99, (total_ - remaining_) * 100 / total_));
        if (progress == last_progress_)
            return;
        last_progress_ = progress;
        progress_callback_(progress_context_, progress);
    }

    NetworkClient &client_;
    size_t remaining_;
    size_t total_;
    ProgressCallback progress_callback_;
    void *progress_context_;
    uint8_t last_progress_ = UINT8_MAX;
    SHA256Builder hash_;
};

__attribute__((noinline))
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

__attribute__((noinline))
bool inflate_entry(
    HashedStream &stream, fs::File &file,
    size_t compressed_size, SHA256Builder &hash,
    size_t &written)
{
    auto allocate_ota_buffer = [](size_t size) -> void *
    {
        void *buffer = heap_caps_malloc(
            size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        return buffer ? buffer : malloc(size);
    };
    auto *dictionary = static_cast<uint8_t *>(
        allocate_ota_buffer(TINFL_LZ_DICT_SIZE));
    auto *input = static_cast<uint8_t *>(
        allocate_ota_buffer(4096));
    auto *decompressor = static_cast<tinfl_decompressor *>(
        allocate_ota_buffer(sizeof(tinfl_decompressor)));
    if (!dictionary || !input || !decompressor)
    {
        free(decompressor);
        free(input);
        free(dictionary);
        return false;
    }
    const auto release_buffers = [&]()
    {
        free(decompressor);
        free(input);
        free(dictionary);
    };
    size_t input_offset = 0;
    size_t input_available = 0;
    size_t compressed_remaining = compressed_size;
    size_t output_offset = 0;
    written = 0;
    tinfl_init(decompressor);
    tinfl_status status = TINFL_STATUS_NEEDS_MORE_INPUT;

    while (status != TINFL_STATUS_DONE)
    {
        if (!input_available)
        {
            if (!compressed_remaining)
            {
                release_buffers();
                return false;
            }
            input_available =
                min(compressed_remaining, size_t(4096));
            if (!stream.readExact(input, input_available))
            {
                release_buffers();
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
            decompressor, input + input_offset, &consumed,
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
                release_buffers();
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
            release_buffers();
            return false;
        }
    }

    const bool exact =
        !input_available && !compressed_remaining;
    release_buffers();
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
    GithubNetworkClientSecure client;
    client.useSystemCertificateBundle();
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
        if (response == 404)
        {
            error = "No published Maclock release was found";
        }
        else if (response < 0)
        {
            error = String("Update connection failed: ") +
                    HTTPClient::errorToString(response);
#ifndef MACLOCK_LOCAL
            char tls_error[160] = {};
            client.lastError(tls_error, sizeof(tls_error));
            if (tls_error[0])
                error += String(" (") + tls_error + ")";
#endif
        }
        else
        {
            error =
                String("Update server returned HTTP ") +
                String(response);
        }
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

    char current_version[
        sizeof(state.snapshot.current_version)] = {};
    portENTER_CRITICAL(&state.mux);
    copy_text(
        current_version, sizeof(current_version),
        state.snapshot.current_version);
    portEXIT_CRITICAL(&state.mux);
    const bool available =
        compare_versions(current_version, version.c_str()) < 0;
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

#endif
