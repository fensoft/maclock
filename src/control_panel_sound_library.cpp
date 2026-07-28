#include "control_panel_sound_library.h"

#include <HTTPClient.h>
#include <LittleFS.h>
#include <NetworkClient.h>

#ifndef MACLOCK_LOCAL
#include <NetworkClientSecure.h>
#endif

#include "sound_selector.h"

namespace
{
static constexpr size_t kMaxSoundFileBytes =
    6U * 1024U * 1024U;
static constexpr const char *kDownloadedSoundDirectory =
    "/downloaded";
static constexpr const char *kSoundUploadTemporaryPath =
    "/downloaded/.maclock-sound-upload.tmp";
static constexpr size_t kMaxMyInstantsResults = 20;

void send_json(
    WebServer &server, JsonDocument &document,
    int status = 200)
{
    String response;
    serializeJson(document, response);
    server.sendHeader("Cache-Control", "no-store");
    server.send(status, "application/json", response);
}

void send_result(
    WebServer &server, bool ok, const char *message,
    int status = 200)
{
    JsonDocument document;
    document["ok"] = ok;
    document["message"] = message;
    send_json(server, document, status);
}

bool equals_ignore_case(
    const char *left, const char *right)
{
    if (!left || !right)
        return left == right;
    while (*left && *right)
    {
        if (tolower(
                static_cast<unsigned char>(*left)) !=
            tolower(static_cast<unsigned char>(*right)))
        {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == *right;
}

bool ends_with_ignore_case(
    const char *text, const char *suffix)
{
    if (!text || !suffix)
        return false;
    const size_t text_length = strlen(text);
    const size_t suffix_length = strlen(suffix);
    return text_length >= suffix_length &&
           equals_ignore_case(
               text + text_length - suffix_length, suffix);
}

bool is_builtin_sound(const char *path)
{
    return equals_ignore_case(path, "/startup.mp3") ||
           equals_ignore_case(path, "/floppy.mp3") ||
           equals_ignore_case(path, "/quack.mp3");
}

bool is_downloaded_sound(const char *path)
{
    if (!path)
        return false;
    static constexpr char kPrefix[] = "/downloaded/";
    return strncmp(path, kPrefix, sizeof(kPrefix) - 1) == 0 &&
           path[sizeof(kPrefix) - 1] != '\0' &&
           strchr(path + sizeof(kPrefix) - 1, '/') == nullptr &&
           ends_with_ignore_case(path, ".mp3");
}

bool ensure_downloaded_sound_directory()
{
    if (LittleFS.exists(kDownloadedSoundDirectory))
        return true;
    return LittleFS.mkdir(kDownloadedSoundDirectory);
}

bool sound_is_in_use(
    const char *path, const ControlPanelSnapshot &snapshot)
{
    if (!path)
        return false;
    if (equals_ignore_case(path, snapshot.startup_sound) ||
        equals_ignore_case(path, snapshot.floppy_sound) ||
        equals_ignore_case(path, snapshot.chime_sound) ||
        equals_ignore_case(path, snapshot.timer.sound))
    {
        return true;
    }
    for (const ControlPanelAlarm &alarm : snapshot.alarms)
    {
        if (equals_ignore_case(path, alarm.sound))
            return true;
    }
    return false;
}

bool read_sound(WebServer &server, String &sound)
{
    if (!server.hasArg("sound"))
        return false;
    sound = server.arg("sound");
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

String sanitize_sound_filename(const char *source)
{
    if (!source)
        return {};
    const char *filename = source;
    for (const char *cursor = source; *cursor; ++cursor)
    {
        if (*cursor == '/' || *cursor == '\\')
            filename = cursor + 1;
    }

    const char *end = filename + strlen(filename);
    for (const char *cursor = filename; cursor < end; ++cursor)
    {
        if (*cursor == '?' || *cursor == '#')
        {
            end = cursor;
            break;
        }
    }
    if (end - filename >= 4 &&
        equals_ignore_case(end - 4, ".mp3"))
    {
        end -= 4;
    }

    char result[72] = {};
    size_t length = 0;
    for (const char *cursor = filename;
         cursor < end && length < sizeof(result) - 5;
         ++cursor)
    {
        const unsigned char value =
            static_cast<unsigned char>(*cursor);
        if (isalnum(value) || value == '-' || value == '_' ||
            value == ' ')
        {
            result[length++] = static_cast<char>(value);
        }
        else if (value == '.')
        {
            result[length++] = '_';
        }
    }
    while (length &&
           (result[length - 1] == ' ' ||
            result[length - 1] == '.'))
    {
        --length;
    }
    if (!length)
        memcpy(result, "sound", 5), length = 5;
    memcpy(result + length, ".mp3", 5);
    return String(result);
}

String unique_sound_path(const String &filename)
{
    char stem[72] = {};
    const size_t length = filename.length();
    const size_t stem_length =
        length > 4 ? min(length - 4, sizeof(stem) - 1) : 0;
    memcpy(stem, filename.c_str(), stem_length);
    stem[stem_length] = '\0';

    if (!ensure_downloaded_sound_directory())
        return {};
    String path =
        String(kDownloadedSoundDirectory) + "/" + filename;
    for (uint8_t suffix = 2;
         LittleFS.exists(path.c_str()) && suffix < 100;
         ++suffix)
    {
        path = String(kDownloadedSoundDirectory) + "/" +
               stem + "-" + String(suffix) + ".mp3";
    }
    return LittleFS.exists(path.c_str()) ? String() : path;
}

bool has_mp3_signature(const char *path)
{
    fs::File file = LittleFS.open(path, "r");
    if (!file)
        return false;
    uint8_t signature[3] = {};
    const size_t read = file.read(signature, sizeof(signature));
    file.close();
    return read == sizeof(signature) &&
           ((signature[0] == 'I' && signature[1] == 'D' &&
             signature[2] == '3') ||
            (signature[0] == 0xFF &&
             (signature[1] & 0xE0) == 0xE0));
}

bool url_has_prefix(
    const String &url, const char *prefix)
{
    const size_t length = strlen(prefix);
    return url.length() >= length &&
           strncmp(url.c_str(), prefix, length) == 0;
}

bool extract_url_host(
    const String &url, char *host, size_t host_size)
{
    const char *start = nullptr;
    if (url_has_prefix(url, "https://"))
        start = url.c_str() + 8;
    else if (url_has_prefix(url, "http://"))
        start = url.c_str() + 7;
    else
        return false;

    const char *end = start;
    while (*end && *end != '/' && *end != ':' &&
           *end != '?' && *end != '#')
    {
        ++end;
    }
    const size_t length = static_cast<size_t>(end - start);
    if (!length || length >= host_size)
        return false;
    memcpy(host, start, length);
    host[length] = '\0';
    for (size_t i = 0; i < length; ++i)
    {
        host[i] = static_cast<char>(
            tolower(static_cast<unsigned char>(host[i])));
    }
    return true;
}

bool public_import_url(const String &url)
{
    char host[96] = {};
    if (!extract_url_host(url, host, sizeof(host)))
        return false;
    const size_t length = strlen(host);
    if (equals_ignore_case(host, "localhost") ||
        (length >= 6 &&
         equals_ignore_case(host + length - 6, ".local")))
    {
        return false;
    }
    bool numeric = true;
    for (const char *cursor = host; *cursor; ++cursor)
    {
        if (!isdigit(static_cast<unsigned char>(*cursor)) &&
            *cursor != '.')
        {
            numeric = false;
            break;
        }
    }
    return !numeric;
}

bool begin_import_http(
    HTTPClient &http, NetworkClient &plain_client,
#ifndef MACLOCK_LOCAL
    NetworkClientSecure &secure_client,
#endif
    const String &url)
{
    NetworkClient *client = &plain_client;
#ifndef MACLOCK_LOCAL
    if (url_has_prefix(url, "https://"))
    {
        secure_client.setInsecure();
        secure_client.setHandshakeTimeout(30);
        client = &secure_client;
    }
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
#endif
    http.useHTTP10(true);
    http.setUserAgent(
        "Maclock/1.0 (+https://github.com/fensoft/maclock)");
    http.setConnectTimeout(10000);
    http.setTimeout(20000);
    return http.begin(*client, url);
}

bool fetch_import_page(
    const String &url, String &payload, String &error)
{
    NetworkClient client;
#ifndef MACLOCK_LOCAL
    NetworkClientSecure secure_client;
#endif
    HTTPClient http;
    if (!begin_import_http(
            http, client,
#ifndef MACLOCK_LOCAL
            secure_client,
#endif
            url))
    {
        error = "Could not open the sound page";
        return false;
    }
    const int response = http.GET();
    if (response != HTTP_CODE_OK)
    {
        error = String("The sound page returned HTTP ") +
                String(response);
#ifndef MACLOCK_LOCAL
        if (response < 0)
        {
            char tls_error[160] = {};
            secure_client.lastError(
                tls_error, sizeof(tls_error));
            if (tls_error[0])
                error += String(": ") + tls_error;
        }
#endif
        http.end();
        return false;
    }
    payload = http.getString();
    http.end();
    if (!payload.length() || payload.length() > 512U * 1024U)
    {
        error = "The sound page is empty or too large";
        return false;
    }
    return true;
}

String percent_encode(const String &value)
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    String result;
    result.reserve(value.length() * 3);
    for (size_t i = 0; i < value.length(); ++i)
    {
        const unsigned char byte =
            static_cast<unsigned char>(value[i]);
        if (isalnum(byte) || byte == '-' || byte == '_' ||
            byte == '.' || byte == '~')
        {
            result += static_cast<char>(byte);
        }
        else
        {
            result += '%';
            result += kHex[byte >> 4];
            result += kHex[byte & 0x0F];
        }
    }
    return result;
}

void append_utf8(String &text, uint32_t codepoint)
{
    if (codepoint <= 0x7F)
    {
        text += static_cast<char>(codepoint);
    }
    else if (codepoint <= 0x7FF)
    {
        text += static_cast<char>(0xC0 | (codepoint >> 6));
        text += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    else if (codepoint <= 0xFFFF)
    {
        text += static_cast<char>(0xE0 | (codepoint >> 12));
        text += static_cast<char>(
            0x80 | ((codepoint >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    else if (codepoint <= 0x10FFFF)
    {
        text += static_cast<char>(0xF0 | (codepoint >> 18));
        text += static_cast<char>(
            0x80 | ((codepoint >> 12) & 0x3F));
        text += static_cast<char>(
            0x80 | ((codepoint >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
}

bool decode_numeric_entity(
    const char *start, const char *end, uint32_t &value)
{
    if (!start || start >= end || *start != '#')
        return false;
    ++start;
    uint8_t base = 10;
    if (start < end && (*start == 'x' || *start == 'X'))
    {
        base = 16;
        ++start;
    }
    if (start >= end)
        return false;

    value = 0;
    for (const char *cursor = start; cursor < end; ++cursor)
    {
        uint8_t digit = 0;
        if (*cursor >= '0' && *cursor <= '9')
            digit = static_cast<uint8_t>(*cursor - '0');
        else if (base == 16 &&
                 *cursor >= 'a' && *cursor <= 'f')
            digit = static_cast<uint8_t>(*cursor - 'a' + 10);
        else if (base == 16 &&
                 *cursor >= 'A' && *cursor <= 'F')
            digit = static_cast<uint8_t>(*cursor - 'A' + 10);
        else
            return false;
        if (digit >= base ||
            value > (0x10FFFFU - digit) / base)
        {
            return false;
        }
        value = value * base + digit;
    }
    return value > 0 && value <= 0x10FFFFU;
}

String decode_html_text(const char *start, const char *end)
{
    String result;
    if (!start || !end || start >= end)
        return result;
    result.reserve(static_cast<size_t>(end - start));
    for (const char *cursor = start; cursor < end;)
    {
        if (*cursor != '&')
        {
            result += *cursor++;
            continue;
        }
        const char *semicolon = cursor + 1;
        while (semicolon < end && *semicolon != ';' &&
               semicolon - cursor <= 12)
        {
            ++semicolon;
        }
        if (semicolon >= end || *semicolon != ';')
        {
            result += *cursor++;
            continue;
        }

        const char *entity = cursor + 1;
        const size_t length =
            static_cast<size_t>(semicolon - entity);
        if (length == 3 && strncmp(entity, "amp", 3) == 0)
            result += '&';
        else if (length == 4 &&
                 strncmp(entity, "quot", 4) == 0)
            result += '"';
        else if (length == 4 &&
                 strncmp(entity, "apos", 4) == 0)
            result += '\'';
        else if (length == 2 &&
                 strncmp(entity, "lt", 2) == 0)
            result += '<';
        else if (length == 2 &&
                 strncmp(entity, "gt", 2) == 0)
            result += '>';
        else
        {
            uint32_t codepoint = 0;
            if (!decode_numeric_entity(
                    entity, semicolon, codepoint))
            {
                for (const char *raw = cursor;
                     raw <= semicolon; ++raw)
                {
                    result += *raw;
                }
                cursor = semicolon + 1;
                continue;
            }
            append_utf8(result, codepoint);
        }
        cursor = semicolon + 1;
    }
    result.trim();
    return result;
}

size_t append_myinstants_results(
    const String &page, JsonArray results)
{
    static constexpr const char *kPlayMarker =
        "onclick=\"play('";
    static constexpr const char *kInstantLinkMarker =
        "<a href=\"/en/instant/";
    static constexpr const char *kMediaPrefix =
        "/media/sounds/";

    const char *cursor = page.c_str();
    const char *page_end = cursor + page.length();
    size_t count = 0;
    while (cursor < page_end && count < kMaxMyInstantsResults)
    {
        const char *play = strstr(cursor, kPlayMarker);
        if (!play)
            break;
        const char *mp3_start = play + strlen(kPlayMarker);
        const char *mp3_end = strchr(mp3_start, '\'');
        if (!mp3_end || mp3_end >= page_end)
            break;
        cursor = mp3_end + 1;

        const char *link = strstr(cursor, kInstantLinkMarker);
        const char *next_play = strstr(cursor, kPlayMarker);
        if (!link || (next_play && next_play < link))
            continue;
        const char *page_url_start = link + strlen("<a href=\"");
        const char *page_url_end = strchr(page_url_start, '"');
        if (!page_url_end)
            break;
        const char *name_start = strchr(page_url_end, '>');
        if (!name_start)
            break;
        ++name_start;
        const char *name_end = strstr(name_start, "</a>");
        if (!name_end)
            break;
        cursor = name_end + 4;

        const size_t mp3_length =
            static_cast<size_t>(mp3_end - mp3_start);
        const size_t page_url_length =
            static_cast<size_t>(page_url_end - page_url_start);
        if (mp3_length <= strlen(kMediaPrefix) + 4 ||
            mp3_length > 240 ||
            page_url_length > 240 ||
            strncmp(
                mp3_start, kMediaPrefix,
                strlen(kMediaPrefix)) != 0 ||
            tolower(static_cast<unsigned char>(mp3_end[-4])) !=
                '.' ||
            tolower(static_cast<unsigned char>(mp3_end[-3])) !=
                'm' ||
            tolower(static_cast<unsigned char>(mp3_end[-2])) !=
                'p' ||
            tolower(static_cast<unsigned char>(mp3_end[-1])) !=
                '3')
        {
            continue;
        }

        const String name = decode_html_text(name_start, name_end);
        if (!name.length())
            continue;
        JsonObject result = results.add<JsonObject>();
        result["name"] = name;
        String mp3_url = "https://www.myinstants.com";
        for (const char *item = mp3_start; item < mp3_end; ++item)
            mp3_url += *item;
        result["mp3Url"] = mp3_url;
        String page_url = "https://www.myinstants.com";
        for (const char *item = page_url_start;
             item < page_url_end; ++item)
        {
            page_url += *item;
        }
        result["pageUrl"] = page_url;
        ++count;
    }
    return count;
}

bool resolve_import_url(
    const String &requested_url, String &mp3_url,
    String &error)
{
    if (!public_import_url(requested_url))
    {
        error = "Enter a public HTTP or HTTPS URL";
        return false;
    }
    const char *query = strchr(requested_url.c_str(), '?');
    const size_t path_length = query
                                   ? static_cast<size_t>(
                                         query -
                                         requested_url.c_str())
                                   : requested_url.length();
    if (path_length >= 4 &&
        equals_ignore_case(
            requested_url.c_str() + path_length - 4, ".mp3"))
    {
        mp3_url = requested_url;
        return true;
    }

    char host[96] = {};
    extract_url_host(requested_url, host, sizeof(host));
    if (!equals_ignore_case(host, "myinstants.com") &&
        !equals_ignore_case(host, "www.myinstants.com"))
    {
        error =
            "Use a direct MP3 URL or a MyInstants sound page";
        return false;
    }

    String page;
    if (!fetch_import_page(requested_url, page, error))
        return false;
    const char *start =
        strstr(page.c_str(), "/media/sounds/");
    if (!start)
    {
        error = "No downloadable MP3 was found on that page";
        return false;
    }
    const char *end = strstr(start, ".mp3");
    if (!end)
    {
        error = "No downloadable MP3 was found on that page";
        return false;
    }
    end += 4;
    if (static_cast<size_t>(end - start) > 240)
    {
        error = "The MP3 link on that page is invalid";
        return false;
    }
    mp3_url = "https://www.myinstants.com";
    for (const char *cursor = start; cursor < end; ++cursor)
        mp3_url += *cursor;
    return true;
}

String filename_from_url(const String &url)
{
    const char *start = strrchr(url.c_str(), '/');
    start = start ? start + 1 : url.c_str();
    return sanitize_sound_filename(start);
}

bool download_import(
    const String &url, const String &suggested_name,
    String &saved_path, String &error)
{
    NetworkClient client;
#ifndef MACLOCK_LOCAL
    NetworkClientSecure secure_client;
#endif
    HTTPClient http;
    if (!begin_import_http(
            http, client,
#ifndef MACLOCK_LOCAL
            secure_client,
#endif
            url))
    {
        error = "Could not connect to the MP3 server";
        return false;
    }
    const int response = http.GET();
    if (response != HTTP_CODE_OK)
    {
        error = "The MP3 could not be downloaded";
        http.end();
        return false;
    }

    LittleFS.remove(kSoundUploadTemporaryPath);
    fs::File file =
        LittleFS.open(kSoundUploadTemporaryPath, "w");
    if (!file)
    {
        error = "Could not create the sound file";
        http.end();
        return false;
    }

    size_t written = 0;
#ifdef MACLOCK_LOCAL
    const String content = http.getString();
    if (content.length() <= kMaxSoundFileBytes)
    {
        written = file.write(
            reinterpret_cast<const uint8_t *>(content.c_str()),
            content.length());
    }
#else
    const int content_length = http.getSize();
    if (content_length > 0 &&
        static_cast<size_t>(content_length) <=
            kMaxSoundFileBytes)
    {
        const int result = http.writeToStream(&file);
        if (result > 0)
            written = static_cast<size_t>(result);
    }
#endif
    file.close();
    http.end();
    if (!written || written > kMaxSoundFileBytes)
    {
        LittleFS.remove(kSoundUploadTemporaryPath);
        error =
            "The MP3 is empty, too large, or has no file size";
        return false;
    }
    if (!has_mp3_signature(kSoundUploadTemporaryPath))
    {
        LittleFS.remove(kSoundUploadTemporaryPath);
        error = "The URL did not return a valid MP3 file";
        return false;
    }

    const String filename = suggested_name.length()
                                ? sanitize_sound_filename(
                                      suggested_name.c_str())
                                : filename_from_url(url);
    saved_path = unique_sound_path(filename);
    if (!saved_path.length() ||
        !LittleFS.rename(
            kSoundUploadTemporaryPath, saved_path.c_str()))
    {
        LittleFS.remove(kSoundUploadTemporaryPath);
        error = "Could not save the downloaded MP3";
        return false;
    }
    SoundSelector::scan();
    return true;
}
} // namespace

void ControlPanelSoundLibrary::appendSnapshot(
    JsonArray sounds,
    const ControlPanelSnapshot &snapshot) const
{
    for (size_t i = 0; i < SoundSelector::count(); ++i)
    {
        const char *path = SoundSelector::pathAt(i);
        if (!path)
            continue;
        JsonObject sound = sounds.add<JsonObject>();
        sound["path"] = path;
        sound["name"] =
            String(SoundSelector::displayName(path));
        fs::File file = LittleFS.open(path, "r");
        sound["size"] = file ? file.size() : 0;
        if (file)
            file.close();
        sound["builtIn"] = is_builtin_sound(path);
        sound["downloaded"] = is_downloaded_sound(path);
        sound["inUse"] = sound_is_in_use(path, snapshot);
    }
}

void ControlPanelSoundLibrary::resetUpload()
{
    if (upload_file_)
        upload_file_.close();
    LittleFS.remove(kSoundUploadTemporaryPath);
    upload_path_.clear();
    upload_error_.clear();
    upload_size_ = 0;
    upload_finished_ = false;
}

void ControlPanelSoundLibrary::receiveUpload(
    WebServer &server)
{
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        resetUpload();
        if (!ensure_downloaded_sound_directory())
        {
            upload_error_ =
                "Could not create the downloaded sounds folder";
            return;
        }
        const String filename =
            sanitize_sound_filename(upload.filename.c_str());
        if (!ends_with_ignore_case(
                upload.filename.c_str(), ".mp3"))
        {
            upload_error_ = "Only MP3 files are accepted";
            return;
        }
        upload_path_ = unique_sound_path(filename);
        if (!upload_path_.length())
        {
            upload_error_ =
                "Could not choose a unique sound filename";
            return;
        }
        upload_file_ =
            LittleFS.open(kSoundUploadTemporaryPath, "w");
        if (!upload_file_)
            upload_error_ = "Could not create the sound file";
        return;
    }

    if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (upload_error_.length() || !upload_file_)
            return;
        if (upload_size_ + upload.currentSize >
            kMaxSoundFileBytes)
        {
            upload_error_ = "MP3 files are limited to 6 MB";
            upload_file_.close();
            LittleFS.remove(kSoundUploadTemporaryPath);
            return;
        }
        const size_t written = upload_file_.write(
            upload.buf, upload.currentSize);
        upload_size_ += written;
        if (written != upload.currentSize)
        {
            upload_error_ =
                "LittleFS does not have enough free space";
            upload_file_.close();
            LittleFS.remove(kSoundUploadTemporaryPath);
        }
        return;
    }

    if (upload.status == UPLOAD_FILE_ABORTED)
    {
        upload_error_ = "Sound upload was cancelled";
        if (upload_file_)
            upload_file_.close();
        LittleFS.remove(kSoundUploadTemporaryPath);
        upload_finished_ = true;
        return;
    }

    if (upload.status != UPLOAD_FILE_END)
        return;
    if (upload_file_)
        upload_file_.close();
    if (!upload_error_.length() &&
        (!upload_size_ ||
         !has_mp3_signature(kSoundUploadTemporaryPath)))
    {
        upload_error_ = "The uploaded file is not a valid MP3";
    }
    if (!upload_error_.length() &&
        !LittleFS.rename(
            kSoundUploadTemporaryPath, upload_path_.c_str()))
    {
        upload_error_ = "Could not finish the sound upload";
    }
    if (upload_error_.length())
        LittleFS.remove(kSoundUploadTemporaryPath);
    else
        SoundSelector::scan();
    upload_finished_ = true;
}

void ControlPanelSoundLibrary::finishUpload(
    WebServer &server)
{
    if (!upload_finished_)
    {
        resetUpload();
        send_result(
            server, false, "No MP3 file was uploaded", 400);
        return;
    }
    if (upload_error_.length())
    {
        send_result(
            server, false, upload_error_.c_str(), 400);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["message"] = "Sound uploaded";
    document["path"] = upload_path_;
    send_json(server, document, 201);
}

void ControlPanelSoundLibrary::importFromUrl(
    WebServer &server, ControlPanelEventSink &events)
{
    String requested_url = server.arg("url");
    requested_url.trim();
    String suggested_name = server.arg("name");
    suggested_name.trim();
    if (suggested_name.length() > 96)
        suggested_name.clear();
    String mp3_url;
    String saved_path;
    String error;
    const bool valid_url =
        requested_url.length() &&
        requested_url.length() <= 512;
    events.beginControlPanelNetworkTransfer();
    const bool imported =
        valid_url &&
        resolve_import_url(requested_url, mp3_url, error) &&
        download_import(
            mp3_url, suggested_name, saved_path, error);
    events.endControlPanelNetworkTransfer();
    if (!imported)
    {
        send_result(
            server, false,
            error.length() ? error.c_str()
                           : "Invalid sound URL",
            400);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["message"] = "Sound imported";
    document["path"] = saved_path;
    send_json(server, document, 201);
}

void ControlPanelSoundLibrary::searchMyInstants(
    WebServer &server, ControlPanelEventSink &events)
{
    String query = server.arg("query");
    query.trim();
    if (query.length() < 2 || query.length() > 80)
    {
        send_result(
            server, false,
            "Search for at least 2 and at most 80 characters",
            400);
        return;
    }

    const String search_url =
        String("https://www.myinstants.com/en/search/?name=") +
        percent_encode(query);
    String page;
    String error;
    events.beginControlPanelNetworkTransfer();
    const bool fetched =
        fetch_import_page(search_url, page, error);
    events.endControlPanelNetworkTransfer();
    if (!fetched)
    {
        send_result(
            server, false,
            error.length() ? error.c_str()
                           : "MyInstants search failed",
            502);
        return;
    }

    JsonDocument document;
    document["ok"] = true;
    document["query"] = query;
    JsonArray results =
        document["results"].to<JsonArray>();
    append_myinstants_results(page, results);
    send_json(server, document);
}

void ControlPanelSoundLibrary::remove(
    WebServer &server, ControlPanelEventSink &events)
{
    String path;
    if (!read_sound(server, path))
    {
        send_result(
            server, false, "Sound file was not found", 404);
        return;
    }
    const ControlPanelSnapshot snapshot =
        events.controlPanelSnapshot();
    if (!is_downloaded_sound(path.c_str()))
    {
        send_result(
            server, false,
            "Only downloaded sounds can be removed", 409);
        return;
    }
    if (sound_is_in_use(path.c_str(), snapshot))
    {
        send_result(
            server, false,
            "Choose another sound everywhere before removing this one",
            409);
        return;
    }
    if (!LittleFS.remove(path.c_str()))
    {
        send_result(
            server, false,
            "Sound file could not be removed", 500);
        return;
    }
    SoundSelector::scan();
    send_result(server, true, "Sound removed");
}
