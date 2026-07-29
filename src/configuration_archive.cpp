#include "configuration_archive.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include <algorithm>
#include <math.h>
#include <strings.h>
#include <time.h>
#include <vector>

#include "audio_volume.h"
#include "brightness.h"
#include "maclock_version.h"
#include "sound_selector.h"

namespace
{
static constexpr char kArchiveFormat[] =
    "maclock-configuration";
static constexpr uint8_t kArchiveVersion = 1;
static constexpr char kConfigurationEntry[] =
    "configuration.json";
static constexpr char kDownloadedPrefix[] =
    "downloaded/";
static constexpr char kFloppyPrefix[] =
    "floppies/";
static constexpr char kRomEntry[] =
    "rom/vMac.ROM";
static constexpr char kRomPath[] =
    "/vMac.ROM";
static constexpr char kRestoreRoot[] =
    "/.maclock-restore";
static constexpr char kRestoreDownloaded[] =
    "/.maclock-restore/downloaded";
static constexpr char kRestoreFloppies[] =
    "/.maclock-restore/floppies";
static constexpr char kRestoreRom[] =
    "/.maclock-restore/vMac.ROM";
static constexpr size_t kIoBufferSize = 4096;
static constexpr size_t kMaxConfigurationBytes =
    64U * 1024U;
static constexpr uint32_t kMaxArchiveEntryBytes =
    32U * 1024U * 1024U;
static constexpr size_t kMaxArchiveEntries = 256;

static uint16_t read_u16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

static uint32_t read_u32(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

static void write_u16(uint8_t *data, uint16_t value)
{
    data[0] = static_cast<uint8_t>(value);
    data[1] = static_cast<uint8_t>(value >> 8);
}

static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = static_cast<uint8_t>(value);
    data[1] = static_cast<uint8_t>(value >> 8);
    data[2] = static_cast<uint8_t>(value >> 16);
    data[3] = static_cast<uint8_t>(value >> 24);
}

static uint32_t crc32_update(
    uint32_t crc, const uint8_t *data, size_t length)
{
    while (length--)
    {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^
                  (0xEDB88320UL &
                   static_cast<uint32_t>(
                       -static_cast<int32_t>(crc & 1)));
    }
    return crc;
}

static bool valid_relative_path(const char *path)
{
    if (!path || !path[0] || path[0] == '/' ||
        strchr(path, '\\') || strstr(path, "..") ||
        strlen(path) >= 160)
    {
        return false;
    }
    for (const char *cursor = path; *cursor; ++cursor)
    {
        const unsigned char value =
            static_cast<unsigned char>(*cursor);
        if (value < 0x20 || value == 0x7F)
            return false;
    }
    return true;
}

static bool is_floppy_name(const char *name)
{
    return name && strlen(name) == 9 &&
           strncasecmp(name, "disk", 4) == 0 &&
           name[4] >= '1' && name[4] <= '9' &&
           strcasecmp(name + 5, ".dsk") == 0;
}

static bool ensure_parent_directories(const char *path)
{
    if (!path || path[0] != '/' || strstr(path, ".."))
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

static void collect_files(
    const char *path, std::vector<String> &files)
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
        const bool directory_entry = entry.isDirectory();
        entry.close();
        const char *name =
            strrchr(entry_path.c_str(), '/');
        name = name ? name + 1 : entry_path.c_str();
        if (name[0] != '.')
        {
            if (directory_entry)
                collect_files(entry_path.c_str(), files);
            else
                files.push_back(entry_path);
        }
        entry = directory.openNextFile();
    }
    directory.close();
}

static void remove_tree(const char *path)
{
    fs::File entry = LittleFS.open(path, "r");
    if (!entry)
        return;
    if (!entry.isDirectory())
    {
        entry.close();
        LittleFS.remove(path);
        return;
    }
    entry.close();

    std::vector<String> children;
    fs::File directory = LittleFS.open(path, "r");
    fs::File child = directory.openNextFile();
    while (child)
    {
        children.push_back(child.path());
        child.close();
        child = directory.openNextFile();
    }
    directory.close();
    for (const String &item : children)
        remove_tree(item.c_str());
    LittleFS.rmdir(path);
}

static bool move_staged_files(
    const char *staging_root,
    const char *destination_root)
{
    std::vector<String> files;
    collect_files(staging_root, files);
    const size_t prefix_length = strlen(staging_root);
    for (const String &source : files)
    {
        const char *suffix =
            source.c_str() + prefix_length;
        const String destination =
            String(destination_root) + suffix;
        if (!ensure_parent_directories(
                destination.c_str()))
        {
            return false;
        }
        if (LittleFS.exists(destination.c_str()))
            LittleFS.remove(destination.c_str());
        if (!LittleFS.rename(
                source.c_str(), destination.c_str()))
        {
            return false;
        }
    }
    return true;
}

static void collect_root_floppies(
    std::vector<String> &files)
{
    fs::File root = LittleFS.open("/", "r");
    if (!root || !root.isDirectory())
    {
        root.close();
        return;
    }
    fs::File entry = root.openNextFile();
    while (entry)
    {
        const String path = entry.path();
        const bool regular = !entry.isDirectory();
        entry.close();
        const char *name = strrchr(path.c_str(), '/');
        name = name ? name + 1 : path.c_str();
        if (regular && is_floppy_name(name))
            files.push_back(path);
        entry = root.openNextFile();
    }
    root.close();
}

static bool replace_restored_files()
{
    remove_tree("/downloaded");
    if (!LittleFS.mkdir("/downloaded") ||
        !move_staged_files(
            kRestoreDownloaded, "/downloaded"))
    {
        return false;
    }

    std::vector<String> existing_floppies;
    collect_root_floppies(existing_floppies);
    for (const String &path : existing_floppies)
    {
        if (!LittleFS.remove(path.c_str()))
            return false;
    }
    if (!move_staged_files(kRestoreFloppies, ""))
        return false;

    if (LittleFS.exists(kRestoreRom))
    {
        if (LittleFS.exists(kRomPath) &&
            !LittleFS.remove(kRomPath))
        {
            return false;
        }
        if (!LittleFS.rename(kRestoreRom, kRomPath))
            return false;
    }
    return true;
}

template <typename T>
static bool read_json_uint(
    JsonVariantConst value, uint32_t minimum,
    uint32_t maximum, T &destination)
{
    if (!value.is<int32_t>())
        return false;
    const int32_t number = value.as<int32_t>();
    if (number < 0 ||
        static_cast<uint32_t>(number) < minimum ||
        static_cast<uint32_t>(number) > maximum)
    {
        return false;
    }
    destination = static_cast<T>(number);
    return true;
}

static bool read_json_bool(
    JsonVariantConst value, bool &destination)
{
    if (!value.is<bool>())
        return false;
    destination = value.as<bool>();
    return true;
}

static bool read_json_text(
    JsonVariantConst value, char *destination,
    size_t destination_size, bool allow_empty = true)
{
    if (!destination || destination_size == 0 ||
        !value.is<const char *>())
    {
        return false;
    }
    const char *text = value.as<const char *>();
    if (!text || (!allow_empty && !text[0]) ||
        strlen(text) >= destination_size)
    {
        return false;
    }
    strlcpy(destination, text, destination_size);
    return true;
}

static void serialize_configuration(
    const ControlPanelConfiguration &configuration,
    String &json)
{
    JsonDocument document;
    document["format"] = kArchiveFormat;
    document["version"] = kArchiveVersion;
    document["firmwareVersion"] = MACLOCK_VERSION;
    document["board"] = MACLOCK_BOARD_ID;

    JsonObject settings =
        document["settings"].to<JsonObject>();
    settings["language"] = static_cast<uint8_t>(
        configuration.settings.language);
    settings["dateFormat"] = static_cast<uint8_t>(
        configuration.settings.date_format);
    settings["temperatureUnit"] = static_cast<uint8_t>(
        configuration.settings.temperature_unit);
    settings["clockFace"] = static_cast<uint8_t>(
        configuration.settings.clock_face);
    settings["clockTheme"] = static_cast<uint8_t>(
        configuration.settings.clock_theme);
    settings["faceAccent"] = static_cast<uint8_t>(
        configuration.settings.face_customization.accent);
    settings["faceNumeralSize"] = static_cast<uint8_t>(
        configuration.settings.face_customization.numeral_size);
    settings["showWeather"] =
        configuration.settings.face_customization.show_weather;
    settings["flipSpeed"] = static_cast<uint8_t>(
        configuration.settings.face_customization.flip_speed);
    settings["hourFormat"] = static_cast<uint8_t>(
        configuration.settings.time_format.hour_format);
    settings["leadingZero"] =
        configuration.settings.time_format.leading_zero;
    settings["showSeconds"] =
        configuration.settings.time_format.show_seconds;
    settings["showWeekday"] =
        configuration.settings.time_format.show_weekday;
    settings["screensaverMode"] = static_cast<uint8_t>(
        configuration.settings.screensaver_mode);
    settings["screensaverDelay"] =
        configuration.settings.screensaver_delay_index;
    settings["bootBrightness"] = static_cast<uint8_t>(
        configuration.settings.boot_brightness);
    settings["bootEmulator"] =
        configuration.settings.boot_floppy_emulator;
    settings["brightness"] = configuration.brightness;

    JsonObject night = settings["night"].to<JsonObject>();
    night["enabled"] =
        configuration.settings.night_mode.enabled;
    night["start"] =
        configuration.settings.night_mode.start_hour;
    night["end"] =
        configuration.settings.night_mode.end_hour;
    night["screenOff"] =
        configuration.settings.night_mode.screen_off_enabled;
    night["offHour"] =
        configuration.settings.night_mode.screen_off_hour;

    JsonObject chime =
        settings["chime"].to<JsonObject>();
    chime["mode"] = static_cast<uint8_t>(
        configuration.settings.chime.mode);
    chime["soundIndex"] =
        configuration.settings.chime.sound;
    chime["sound"] = configuration.chime_sound;
    chime["volume"] =
        configuration.settings.chime.volume;
    chime["quiet"] =
        configuration.settings.chime.quiet_enabled;
    chime["quietStart"] =
        configuration.settings.chime.quiet_start_hour;
    chime["quietEnd"] =
        configuration.settings.chime.quiet_end_hour;

    JsonObject system_sounds =
        settings["systemSounds"].to<JsonObject>();
    system_sounds["startup"] =
        configuration.startup_sound;
    system_sounds["startupVolume"] =
        configuration.startup_volume;
    system_sounds["floppy"] =
        configuration.floppy_sound;
    system_sounds["floppyVolume"] =
        configuration.floppy_volume;

    JsonObject timer =
        settings["timer"].to<JsonObject>();
    timer["minutes"] = configuration.timer.minutes;
    timer["sound"] = configuration.timer.sound;
    timer["volume"] = configuration.timer.volume;

    JsonArray alarms = document["alarms"].to<JsonArray>();
    for (const ControlPanelAlarm &alarm :
         configuration.alarms)
    {
        JsonObject item = alarms.add<JsonObject>();
        item["enabled"] = alarm.enabled != 0;
        item["hour"] = alarm.hour;
        item["minute"] = alarm.minute;
        item["weekdays"] = alarm.weekdays;
        item["sound"] = alarm.sound;
        item["volume"] = alarm.volume;
        item["oneTime"] = alarm.one_time;
        item["gradualVolume"] =
            alarm.gradual_volume;
        item["sunrise"] = alarm.sunrise;
        item["label"] = alarm.label;
    }

    JsonObject wifi = document["wifi"].to<JsonObject>();
    wifi["enabled"] = configuration.wifi.enabled;
    wifi["ssid"] = configuration.wifi.ssid;
    wifi["city"] = configuration.wifi.city;
    wifi["country"] = configuration.wifi.country;
    wifi["coordinatesValid"] =
        configuration.wifi.coordinates_valid;
    wifi["latitude"] = configuration.wifi.latitude;
    wifi["longitude"] = configuration.wifi.longitude;
    wifi["location"] = configuration.wifi.location;
    wifi["timezone"] = configuration.wifi.timezone;
    wifi["utcOffsetSeconds"] =
        configuration.wifi.utc_offset_seconds;

    if (configuration.touch.valid)
    {
        JsonObject touch =
            document["touchCalibration"].to<JsonObject>();
        touch["minX"] = configuration.touch.min_x;
        touch["maxX"] = configuration.touch.max_x;
        touch["minY"] = configuration.touch.min_y;
        touch["maxY"] = configuration.touch.max_y;
    }
    else
    {
        document["touchCalibration"] = nullptr;
    }
    serializeJsonPretty(document, json);
}

static bool deserialize_configuration(
    const String &json,
    ControlPanelConfiguration &configuration,
    String &error)
{
    JsonDocument document;
    const DeserializationError json_error =
        deserializeJson(document, json);
    if (json_error)
    {
        error = "configuration.json is not valid JSON";
        return false;
    }
    if (strcmp(
            document["format"] | "",
            kArchiveFormat) != 0 ||
        (document["version"] | 0) != kArchiveVersion)
    {
        error = "Unsupported Maclock backup format";
        return false;
    }

    JsonObjectConst settings = document["settings"];
    JsonObjectConst night = settings["night"];
    JsonObjectConst chime = settings["chime"];
    JsonObjectConst system_sounds =
        settings["systemSounds"];
    JsonObjectConst timer = settings["timer"];
    if (settings.isNull() || night.isNull() ||
        chime.isNull() || system_sounds.isNull() ||
        timer.isNull())
    {
        error = "The backup is missing settings";
        return false;
    }

    uint8_t value = 0;
    bool boolean = false;
    if (!read_json_uint(
            settings["language"], 0,
            UI_LANGUAGE_COUNT - 1, value))
        goto invalid_settings;
    configuration.settings.language =
        static_cast<UiLanguage>(value);
    if (!read_json_uint(
            settings["dateFormat"], 0,
            UI_DATE_FORMAT_COUNT - 1, value))
        goto invalid_settings;
    configuration.settings.date_format =
        static_cast<UiDateFormat>(value);
    if (!read_json_uint(
            settings["temperatureUnit"], 0,
            UI_TEMPERATURE_UNIT_COUNT - 1, value))
        goto invalid_settings;
    configuration.settings.temperature_unit =
        static_cast<UiTemperatureUnit>(value);
    if (!read_json_uint(
            settings["clockFace"], 0,
            static_cast<uint8_t>(ClockFace::Count) - 1,
            value))
        goto invalid_settings;
    configuration.settings.clock_face =
        static_cast<ClockFace>(value);
    if (!read_json_uint(
            settings["clockTheme"], 0,
            static_cast<uint8_t>(ClockTheme::Count) - 1,
            value))
        goto invalid_settings;
    configuration.settings.clock_theme =
        static_cast<ClockTheme>(value);
    if (!read_json_uint(
            settings["faceAccent"], 0,
            static_cast<uint8_t>(FaceAccent::Count) - 1,
            value))
        goto invalid_settings;
    configuration.settings.face_customization.accent =
        static_cast<FaceAccent>(value);
    if (!read_json_uint(
            settings["faceNumeralSize"], 0,
            static_cast<uint8_t>(
                FaceNumeralSize::Count) -
                1,
            value))
        goto invalid_settings;
    configuration.settings.face_customization.numeral_size =
        static_cast<FaceNumeralSize>(value);
    if (!read_json_bool(
            settings["showWeather"], boolean))
        goto invalid_settings;
    configuration.settings.face_customization.show_weather =
        boolean;
    if (!read_json_uint(
            settings["flipSpeed"], 0,
            static_cast<uint8_t>(
                FlipAnimationSpeed::Count) -
                1,
            value))
        goto invalid_settings;
    configuration.settings.face_customization.flip_speed =
        static_cast<FlipAnimationSpeed>(value);
    if (!read_json_uint(
            settings["hourFormat"], 0,
            static_cast<uint8_t>(HourFormat::Count) - 1,
            value))
        goto invalid_settings;
    configuration.settings.time_format.hour_format =
        static_cast<HourFormat>(value);
    if (!read_json_bool(
            settings["leadingZero"], boolean))
        goto invalid_settings;
    configuration.settings.time_format.leading_zero =
        boolean;
    if (!read_json_bool(
            settings["showSeconds"], boolean))
        goto invalid_settings;
    configuration.settings.time_format.show_seconds =
        boolean;
    if (!read_json_bool(
            settings["showWeekday"], boolean))
        goto invalid_settings;
    configuration.settings.time_format.show_weekday =
        boolean;
    if (!read_json_uint(
            settings["screensaverMode"], 0,
            static_cast<uint8_t>(
                ScreensaverMode::Count) -
                1,
            value))
        goto invalid_settings;
    configuration.settings.screensaver_mode =
        static_cast<ScreensaverMode>(value);
    if (!read_json_uint(
            settings["screensaverDelay"], 0,
            kScreensaverDelayCount - 1, value))
        goto invalid_settings;
    configuration.settings.screensaver_delay_index =
        value;
    if (!read_json_uint(
            settings["bootBrightness"], 0,
            static_cast<uint8_t>(
                BootBrightness::Highest),
            value))
        goto invalid_settings;
    configuration.settings.boot_brightness =
        static_cast<BootBrightness>(value);
    if (!read_json_bool(
            settings["bootEmulator"], boolean))
        goto invalid_settings;
    configuration.settings.boot_floppy_emulator =
        boolean;
    if (!read_json_uint(
            settings["brightness"], 0,
            kBrightnessMax, configuration.brightness))
        goto invalid_settings;

    if (!read_json_bool(night["enabled"], boolean))
        goto invalid_settings;
    configuration.settings.night_mode.enabled = boolean;
    if (!read_json_uint(
            night["start"], 0, 23,
            configuration.settings.night_mode.start_hour) ||
        !read_json_uint(
            night["end"], 0, 23,
            configuration.settings.night_mode.end_hour) ||
        !read_json_bool(
            night["screenOff"], boolean) ||
        !read_json_uint(
            night["offHour"], 0, 23,
            configuration.settings.night_mode
                .screen_off_hour))
        goto invalid_settings;
    configuration.settings.night_mode.screen_off_enabled =
        boolean;

    if (!read_json_uint(
            chime["mode"], 0,
            static_cast<uint8_t>(ChimeMode::Count) - 1,
            value))
        goto invalid_settings;
    configuration.settings.chime.mode =
        static_cast<ChimeMode>(value);
    if (!read_json_uint(
            chime["soundIndex"], 0, 2,
            configuration.settings.chime.sound) ||
        !read_json_text(
            chime["sound"], configuration.chime_sound,
            sizeof(configuration.chime_sound), false) ||
        !read_json_uint(
            chime["volume"], 0,
            kAudioVolumeLevelCount - 1,
            configuration.settings.chime.volume) ||
        !read_json_bool(chime["quiet"], boolean))
        goto invalid_settings;
    configuration.settings.chime.quiet_enabled = boolean;
    if (!read_json_uint(
            chime["quietStart"], 0, 23,
            configuration.settings.chime
                .quiet_start_hour) ||
        !read_json_uint(
            chime["quietEnd"], 0, 23,
            configuration.settings.chime
                .quiet_end_hour))
        goto invalid_settings;

    if (!read_json_text(
            system_sounds["startup"],
            configuration.startup_sound,
            sizeof(configuration.startup_sound), false) ||
        !read_json_uint(
            system_sounds["startupVolume"], 10, 100,
            configuration.startup_volume) ||
        !audio_volume_is_level(
            configuration.startup_volume) ||
        !read_json_text(
            system_sounds["floppy"],
            configuration.floppy_sound,
            sizeof(configuration.floppy_sound), false) ||
        !read_json_uint(
            system_sounds["floppyVolume"], 10, 100,
            configuration.floppy_volume) ||
        !audio_volume_is_level(
            configuration.floppy_volume) ||
        !read_json_uint(
            timer["minutes"], 1, 1440,
            configuration.timer.minutes) ||
        !read_json_text(
            timer["sound"], configuration.timer.sound,
            sizeof(configuration.timer.sound), false) ||
        !read_json_uint(
            timer["volume"], 0,
            kAudioVolumeLevelCount - 1,
            configuration.timer.volume))
        goto invalid_settings;

    {
        JsonArrayConst alarms = document["alarms"];
        if (alarms.isNull() ||
            alarms.size() != kControlPanelAlarmCount)
        {
            error = "The backup must contain three alarms";
            return false;
        }
        size_t index = 0;
        for (JsonObjectConst item : alarms)
        {
            ControlPanelAlarm &alarm =
                configuration.alarms[index++];
            if (!read_json_bool(item["enabled"], boolean))
                goto invalid_alarms;
            alarm.enabled = boolean ? 1 : 0;
            if (!read_json_uint(
                    item["hour"], 0, 23, alarm.hour) ||
                !read_json_uint(
                    item["minute"], 0, 59,
                    alarm.minute) ||
                !read_json_uint(
                    item["weekdays"], 0, 0x7F,
                    alarm.weekdays) ||
                !read_json_text(
                    item["sound"], alarm.sound,
                    sizeof(alarm.sound), false) ||
                !read_json_uint(
                    item["volume"], 0,
                    kAudioVolumeLevelCount - 1,
                    alarm.volume) ||
                !read_json_bool(
                    item["oneTime"], alarm.one_time) ||
                !read_json_bool(
                    item["gradualVolume"],
                    alarm.gradual_volume) ||
                !read_json_bool(
                    item["sunrise"], alarm.sunrise) ||
                !read_json_text(
                    item["label"], alarm.label,
                    sizeof(alarm.label)))
            {
                goto invalid_alarms;
            }
        }
    }

    {
        JsonObjectConst wifi = document["wifi"];
        if (wifi.isNull() ||
            !read_json_bool(
                wifi["enabled"],
                configuration.wifi.enabled) ||
            !read_json_text(
                wifi["ssid"], configuration.wifi.ssid,
                sizeof(configuration.wifi.ssid)) ||
            !read_json_text(
                wifi["city"], configuration.wifi.city,
                sizeof(configuration.wifi.city)) ||
            !read_json_text(
                wifi["country"],
                configuration.wifi.country,
                sizeof(configuration.wifi.country)) ||
            !read_json_bool(
                wifi["coordinatesValid"],
                configuration.wifi.coordinates_valid) ||
            !wifi["latitude"].is<double>() ||
            !wifi["longitude"].is<double>() ||
            !read_json_text(
                wifi["location"],
                configuration.wifi.location,
                sizeof(configuration.wifi.location)) ||
            !read_json_text(
                wifi["timezone"],
                configuration.wifi.timezone,
                sizeof(configuration.wifi.timezone)) ||
            !wifi["utcOffsetSeconds"].is<int32_t>())
        {
            error = "The backup contains invalid Wi-Fi settings";
            return false;
        }
        configuration.wifi.latitude =
            wifi["latitude"].as<double>();
        configuration.wifi.longitude =
            wifi["longitude"].as<double>();
        configuration.wifi.utc_offset_seconds =
            wifi["utcOffsetSeconds"].as<int32_t>();
        String country = configuration.wifi.country;
        country.trim();
        country.toUpperCase();
        if ((country.length() != 0 &&
             country.length() != 2) ||
            configuration.wifi.latitude < -90.0 ||
            configuration.wifi.latitude > 90.0 ||
            configuration.wifi.longitude < -180.0 ||
            configuration.wifi.longitude > 180.0 ||
            !isfinite(configuration.wifi.latitude) ||
            !isfinite(configuration.wifi.longitude))
        {
            error = "The backup contains invalid Wi-Fi settings";
            return false;
        }
        country.toCharArray(
            configuration.wifi.country,
            sizeof(configuration.wifi.country));
    }

    if (document["touchCalibration"].isNull())
    {
        configuration.touch = TouchCalibration();
    }
    else
    {
        JsonObjectConst touch =
            document["touchCalibration"];
        configuration.touch.valid = true;
        if (touch.isNull() ||
            !read_json_uint(
                touch["minX"], 0, UINT16_MAX,
                configuration.touch.min_x) ||
            !read_json_uint(
                touch["maxX"], 0, UINT16_MAX,
                configuration.touch.max_x) ||
            !read_json_uint(
                touch["minY"], 0, UINT16_MAX,
                configuration.touch.min_y) ||
            !read_json_uint(
                touch["maxY"], 0, UINT16_MAX,
                configuration.touch.max_y) ||
            configuration.touch.min_x >=
                configuration.touch.max_x ||
            configuration.touch.min_y >=
                configuration.touch.max_y)
        {
            error = "The backup contains invalid touch calibration";
            return false;
        }
    }
    return true;

invalid_settings:
    error = "The backup contains invalid settings";
    return false;
invalid_alarms:
    error = "The backup contains invalid alarm settings";
    return false;
}

struct ExportEntry
{
    String archive_name;
    String source_path;
    String content;
    uint32_t size = 0;
    uint32_t crc = 0;
    uint32_t local_offset = 0;
    bool memory = false;
};

static bool calculate_file_crc(
    ExportEntry &entry, String &error)
{
    fs::File file =
        LittleFS.open(entry.source_path.c_str(), "r");
    if (!file || file.isDirectory() ||
        file.size() > UINT32_MAX)
    {
        file.close();
        error = String("Could not read ") +
                entry.source_path;
        return false;
    }
    entry.size = static_cast<uint32_t>(file.size());
    uint8_t buffer[kIoBufferSize];
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t remaining = entry.size;
    while (remaining)
    {
        const size_t requested =
            std::min<size_t>(
                sizeof(buffer), remaining);
        const size_t count =
            file.read(buffer, requested);
        if (count == 0)
        {
            file.close();
            error = String("Could not read ") +
                    entry.source_path;
            return false;
        }
        crc = crc32_update(crc, buffer, count);
        remaining -= count;
    }
    file.close();
    entry.crc = crc ^ 0xFFFFFFFFUL;
    return true;
}

static bool build_export_entries(
    ControlPanelEventSink &events,
    std::vector<ExportEntry> &entries,
    String &error)
{
    ExportEntry configuration;
    configuration.archive_name = kConfigurationEntry;
    configuration.memory = true;
    serialize_configuration(
        events.controlPanelConfiguration(),
        configuration.content);
    configuration.size =
        static_cast<uint32_t>(
            configuration.content.length());
    configuration.crc =
        crc32_update(
            0xFFFFFFFFUL,
            reinterpret_cast<const uint8_t *>(
                configuration.content.c_str()),
            configuration.content.length()) ^
        0xFFFFFFFFUL;
    entries.push_back(configuration);

    std::vector<String> downloaded;
    collect_files("/downloaded", downloaded);
    for (const String &path : downloaded)
    {
        ExportEntry entry;
        entry.source_path = path;
        entry.archive_name =
            String(kDownloadedPrefix) +
            path.substring(strlen("/downloaded/"));
        if (!calculate_file_crc(entry, error))
            return false;
        entries.push_back(entry);
    }

    std::vector<String> floppies;
    collect_root_floppies(floppies);
    for (const String &path : floppies)
    {
        ExportEntry entry;
        entry.source_path = path;
        const char *name =
            strrchr(path.c_str(), '/');
        entry.archive_name =
            String(kFloppyPrefix) +
            (name ? name + 1 : path.c_str());
        if (!calculate_file_crc(entry, error))
            return false;
        entries.push_back(entry);
    }

    if (LittleFS.exists(kRomPath))
    {
        ExportEntry entry;
        entry.source_path = kRomPath;
        entry.archive_name = kRomEntry;
        if (!calculate_file_crc(entry, error))
            return false;
        entries.push_back(entry);
    }

    if (entries.size() > kMaxArchiveEntries)
    {
        error = "Too many files to back up";
        return false;
    }
    return true;
}

static void send_binary(
    WebServer &server, const uint8_t *data,
    size_t length, uint32_t &offset)
{
    server.sendContent(
        reinterpret_cast<const char *>(data), length);
    offset += static_cast<uint32_t>(length);
}

static void stream_zip(
    WebServer &server, std::vector<ExportEntry> &entries)
{
    uint32_t offset = 0;
    uint8_t header[46] = {};
    uint8_t buffer[kIoBufferSize];

    for (ExportEntry &entry : entries)
    {
        entry.local_offset = offset;
        memset(header, 0, 30);
        write_u32(header, 0x04034B50UL);
        write_u16(header + 4, 20);
        write_u16(header + 6, 0x0800);
        write_u16(header + 8, 0);
        write_u32(header + 14, entry.crc);
        write_u32(header + 18, entry.size);
        write_u32(header + 22, entry.size);
        write_u16(
            header + 26,
            static_cast<uint16_t>(
                entry.archive_name.length()));
        send_binary(server, header, 30, offset);
        send_binary(
            server,
            reinterpret_cast<const uint8_t *>(
                entry.archive_name.c_str()),
            entry.archive_name.length(), offset);

        if (entry.memory)
        {
            send_binary(
                server,
                reinterpret_cast<const uint8_t *>(
                    entry.content.c_str()),
                entry.content.length(), offset);
            continue;
        }

        fs::File file =
            LittleFS.open(entry.source_path.c_str(), "r");
        uint32_t remaining = entry.size;
        while (file && remaining)
        {
            const size_t count = file.read(
                buffer,
                std::min<size_t>(
                    sizeof(buffer), remaining));
            if (!count)
                break;
            send_binary(server, buffer, count, offset);
            remaining -= count;
        }
        file.close();
    }

    const uint32_t central_offset = offset;
    for (const ExportEntry &entry : entries)
    {
        memset(header, 0, sizeof(header));
        write_u32(header, 0x02014B50UL);
        write_u16(header + 4, 20);
        write_u16(header + 6, 20);
        write_u16(header + 8, 0x0800);
        write_u16(header + 10, 0);
        write_u32(header + 16, entry.crc);
        write_u32(header + 20, entry.size);
        write_u32(header + 24, entry.size);
        write_u16(
            header + 28,
            static_cast<uint16_t>(
                entry.archive_name.length()));
        write_u32(header + 42, entry.local_offset);
        send_binary(server, header, sizeof(header), offset);
        send_binary(
            server,
            reinterpret_cast<const uint8_t *>(
                entry.archive_name.c_str()),
            entry.archive_name.length(), offset);
    }

    const uint32_t central_size = offset - central_offset;
    uint8_t end[22] = {};
    write_u32(end, 0x06054B50UL);
    write_u16(
        end + 8,
        static_cast<uint16_t>(entries.size()));
    write_u16(
        end + 10,
        static_cast<uint16_t>(entries.size()));
    write_u32(end + 12, central_size);
    write_u32(end + 16, central_offset);
    send_binary(server, end, sizeof(end), offset);
}

class RestoreParser
{
public:
    bool begin()
    {
        abort();
        remove_tree(kRestoreRoot);
        if (!LittleFS.mkdir(kRestoreRoot) ||
            !LittleFS.mkdir(kRestoreDownloaded) ||
            !LittleFS.mkdir(kRestoreFloppies))
        {
            fail("Could not create restore staging folders");
            return false;
        }
        state_ = ParserState::Signature;
        return true;
    }

    bool feed(const uint8_t *data, size_t length)
    {
        if (!data || state_ == ParserState::Failed)
            return false;
        while (length && state_ != ParserState::Failed)
        {
            if (state_ == ParserState::Trailing)
            {
                for (size_t index = 0; index < length; ++index)
                    push_tail(data[index]);
                stream_offset_ += length;
                return true;
            }
            if (state_ == ParserState::Data)
            {
                const size_t count =
                    std::min<size_t>(
                        length, data_remaining_);
                if (output_ &&
                    output_.write(data, count) != count)
                {
                    fail("Could not write a restored file");
                    return false;
                }
                if (current_is_configuration_)
                {
                    for (size_t index = 0;
                         index < count; ++index)
                    {
                        configuration_ +=
                            static_cast<char>(data[index]);
                    }
                }
                current_crc_ =
                    crc32_update(current_crc_, data, count);
                data += count;
                length -= count;
                stream_offset_ += count;
                data_remaining_ -= count;
                if (!data_remaining_ && !finish_entry())
                    return false;
                continue;
            }

            const uint8_t byte = *data++;
            --length;
            ++stream_offset_;
            if (state_ == ParserState::Signature)
            {
                field_[field_length_++] = byte;
                if (field_length_ != 4)
                    continue;
                const uint32_t signature =
                    read_u32(field_);
                field_length_ = 0;
                if (signature == 0x04034B50UL)
                {
                    record_offset_ = stream_offset_ - 4;
                    state_ = ParserState::Header;
                }
                else if (signature == 0x02014B50UL)
                {
                    central_offset_ = stream_offset_ - 4;
                    state_ = ParserState::Trailing;
                    push_tail(0x50);
                    push_tail(0x4B);
                    push_tail(0x01);
                    push_tail(0x02);
                }
                else
                {
                    fail("The ZIP archive has an invalid record");
                }
                continue;
            }
            if (state_ == ParserState::Header)
            {
                field_[field_length_++] = byte;
                if (field_length_ == 26)
                {
                    if (!start_header())
                        return false;
                }
                continue;
            }
            if (state_ == ParserState::Name)
            {
                name_ += static_cast<char>(byte);
                if (--name_remaining_ == 0)
                {
                    state_ = extra_remaining_
                                 ? ParserState::Extra
                                 : ParserState::Data;
                    if (!extra_remaining_ &&
                        !open_entry())
                    {
                        return false;
                    }
                }
                continue;
            }
            if (state_ == ParserState::Extra)
            {
                if (--extra_remaining_ == 0)
                {
                    state_ = ParserState::Data;
                    if (!open_entry())
                        return false;
                }
            }
        }
        return state_ != ParserState::Failed;
    }

    bool finish()
    {
        if (output_)
            output_.close();
        if (state_ != ParserState::Trailing ||
            tail_length_ != sizeof(tail_) ||
            read_u32(tail_) != 0x06054B50UL ||
            read_u16(tail_ + 4) != 0 ||
            read_u16(tail_ + 6) != 0 ||
            read_u16(tail_ + 8) != entry_count_ ||
            read_u16(tail_ + 10) != entry_count_ ||
            read_u32(tail_ + 16) != central_offset_ ||
            read_u16(tail_ + 20) != 0 ||
            static_cast<uint64_t>(
                read_u32(tail_ + 12)) +
                    central_offset_ + sizeof(tail_) !=
                stream_offset_ ||
            !configuration_seen_)
        {
            fail("The ZIP archive is incomplete or invalid");
            return false;
        }
        return true;
    }

    void abort()
    {
        if (output_)
            output_.close();
        remove_tree(kRestoreRoot);
        state_ = ParserState::Failed;
        error_.clear();
        configuration_.clear();
        names_.clear();
        field_length_ = 0;
        tail_length_ = 0;
        stream_offset_ = 0;
        central_offset_ = 0;
        entry_count_ = 0;
        configuration_seen_ = false;
    }

    const String &configuration() const
    {
        return configuration_;
    }

    const String &error() const
    {
        return error_;
    }

private:
    enum class ParserState : uint8_t
    {
        Signature,
        Header,
        Name,
        Extra,
        Data,
        Trailing,
        Failed
    };

    void fail(const char *message)
    {
        if (output_)
            output_.close();
        error_ = message;
        state_ = ParserState::Failed;
    }

    bool start_header()
    {
        const uint16_t flags = read_u16(field_ + 2);
        const uint16_t method = read_u16(field_ + 4);
        expected_crc_ = read_u32(field_ + 10);
        const uint32_t compressed_size =
            read_u32(field_ + 14);
        data_remaining_ = read_u32(field_ + 18);
        name_remaining_ = read_u16(field_ + 22);
        extra_remaining_ = read_u16(field_ + 24);
        field_length_ = 0;
        if ((flags & ~0x0800U) != 0 ||
            method != 0 ||
            compressed_size != data_remaining_ ||
            data_remaining_ > kMaxArchiveEntryBytes ||
            name_remaining_ == 0 ||
            name_remaining_ >= 160 ||
            extra_remaining_ > 1024 ||
            entry_count_ >= kMaxArchiveEntries)
        {
            fail("The ZIP archive uses unsupported features");
            return false;
        }
        name_.clear();
        name_.reserve(name_remaining_);
        state_ = ParserState::Name;
        return true;
    }

    bool duplicate_name() const
    {
        for (const String &existing : names_)
        {
            if (existing == name_)
                return true;
        }
        return false;
    }

    bool open_entry()
    {
        if (duplicate_name())
        {
            fail("The ZIP archive contains duplicate files");
            return false;
        }
        names_.push_back(name_);
        current_crc_ = 0xFFFFFFFFUL;
        current_is_configuration_ = false;
        if (name_ == kConfigurationEntry)
        {
            if (configuration_seen_ ||
                data_remaining_ > kMaxConfigurationBytes)
            {
                fail("configuration.json is invalid");
                return false;
            }
            configuration_seen_ = true;
            current_is_configuration_ = true;
            configuration_.clear();
            configuration_.reserve(data_remaining_);
        }
        else if (name_.startsWith(kDownloadedPrefix))
        {
            const String relative =
                name_.substring(strlen(kDownloadedPrefix));
            if (!valid_relative_path(relative.c_str()))
            {
                fail("The backup contains an invalid downloaded path");
                return false;
            }
            const String path =
                String(kRestoreDownloaded) + "/" + relative;
            if (!ensure_parent_directories(path.c_str()))
            {
                fail("Could not create restore folders");
                return false;
            }
            output_ = LittleFS.open(path.c_str(), "w");
            if (!output_)
            {
                fail("Could not stage a downloaded file");
                return false;
            }
        }
        else if (name_.startsWith(kFloppyPrefix))
        {
            const String relative =
                name_.substring(strlen(kFloppyPrefix));
            if (!valid_relative_path(relative.c_str()) ||
                strchr(relative.c_str(), '/') ||
                !is_floppy_name(relative.c_str()))
            {
                fail("The backup contains an invalid floppy path");
                return false;
            }
            const String path =
                String(kRestoreFloppies) + "/" + relative;
            output_ = LittleFS.open(path.c_str(), "w");
            if (!output_)
            {
                fail("Could not stage a floppy image");
                return false;
            }
        }
        else if (name_ == kRomEntry)
        {
            output_ = LittleFS.open(kRestoreRom, "w");
            if (!output_)
            {
                fail("Could not stage the Macintosh ROM");
                return false;
            }
        }
        else
        {
            fail("The ZIP archive contains an unexpected file");
            return false;
        }
        return data_remaining_ ? true : finish_entry();
    }

    bool finish_entry()
    {
        if (output_)
            output_.close();
        const uint32_t actual_crc =
            current_crc_ ^ 0xFFFFFFFFUL;
        if (actual_crc != expected_crc_)
        {
            fail("A ZIP entry failed its CRC check");
            return false;
        }
        ++entry_count_;
        current_is_configuration_ = false;
        state_ = ParserState::Signature;
        return true;
    }

    void push_tail(uint8_t byte)
    {
        if (tail_length_ < sizeof(tail_))
        {
            tail_[tail_length_++] = byte;
            return;
        }
        memmove(tail_, tail_ + 1, sizeof(tail_) - 1);
        tail_[sizeof(tail_) - 1] = byte;
    }

    ParserState state_ = ParserState::Failed;
    fs::File output_;
    String configuration_;
    String error_;
    String name_;
    std::vector<String> names_;
    uint8_t field_[26] = {};
    size_t field_length_ = 0;
    uint8_t tail_[22] = {};
    size_t tail_length_ = 0;
    uint32_t stream_offset_ = 0;
    uint32_t record_offset_ = 0;
    uint32_t central_offset_ = 0;
    uint32_t expected_crc_ = 0;
    uint32_t current_crc_ = 0;
    uint32_t data_remaining_ = 0;
    uint16_t name_remaining_ = 0;
    uint16_t extra_remaining_ = 0;
    uint16_t entry_count_ = 0;
    bool current_is_configuration_ = false;
    bool configuration_seen_ = false;
};

static void append_sound_warning(
    char *path, size_t path_size,
    const char *fallback, const char *field,
    std::vector<String> &warnings)
{
    const char *resolved =
        SoundSelector::resolvePath(path, fallback);
    if (strcasecmp(resolved, path) == 0)
        return;
    warnings.push_back(
        String(field) + " used a missing sound; " +
        fallback + " was selected");
    strlcpy(path, fallback, path_size);
}

static void resolve_restored_sounds(
    ControlPanelConfiguration &configuration,
    std::vector<String> &warnings)
{
    SoundSelector::scan();
    append_sound_warning(
        configuration.startup_sound,
        sizeof(configuration.startup_sound),
        "/startup.mp3", "Startup sound", warnings);
    append_sound_warning(
        configuration.floppy_sound,
        sizeof(configuration.floppy_sound),
        "/floppy.mp3", "Floppy sound", warnings);
    append_sound_warning(
        configuration.chime_sound,
        sizeof(configuration.chime_sound),
        "/quack.mp3", "Chime sound", warnings);
    append_sound_warning(
        configuration.timer.sound,
        sizeof(configuration.timer.sound),
        "/quack.mp3", "Timer sound", warnings);
    for (size_t index = 0;
         index < kControlPanelAlarmCount; ++index)
    {
        char field[24];
        snprintf(
            field, sizeof(field), "Alarm %u sound",
            static_cast<unsigned>(index + 1));
        append_sound_warning(
            configuration.alarms[index].sound,
            sizeof(configuration.alarms[index].sound),
            "/quack.mp3", field, warnings);
    }
}

static void send_result(
    WebServer &server, bool ok, const char *message,
    const std::vector<String> &warnings,
    bool network_changed, int status)
{
    JsonDocument document;
    document["ok"] = ok;
    document["message"] = message;
    document["networkChanged"] = network_changed;
    JsonArray warning_array =
        document["warnings"].to<JsonArray>();
    for (const String &warning : warnings)
        warning_array.add(warning);
    String response;
    serializeJson(document, response);
    server.sendHeader("Cache-Control", "no-store");
    server.send(status, "application/json", response);
}
} // namespace

struct ConfigurationArchive::State
{
    ControlPanelEventSink *events = nullptr;
    RestoreParser restore;
    bool upload_started = false;
    bool upload_finished = false;
    bool transfer_active = false;
    String upload_error;
};

void ConfigurationArchive::begin(
    ControlPanelEventSink &events)
{
    if (!state_)
        state_ = new State();
    state_->events = &events;
}

void ConfigurationArchive::sendExport(WebServer &server)
{
    if (!state_ || !state_->events)
    {
        std::vector<String> warnings;
        send_result(
            server, false,
            "Control service is unavailable",
            warnings, false, 503);
        return;
    }

    state_->events->beginControlPanelNetworkTransfer();
    std::vector<ExportEntry> entries;
    String error;
    if (!build_export_entries(
            *state_->events, entries, error))
    {
        state_->events->endControlPanelNetworkTransfer();
        std::vector<String> warnings;
        send_result(
            server, false, error.c_str(),
            warnings, false, 500);
        return;
    }

    char timestamp[24] = {};
    const time_t now = time(nullptr);
    struct tm local_time = {};
    if (localtime_r(&now, &local_time))
    {
        strftime(
            timestamp, sizeof(timestamp),
            "%Y-%m-%d_%H-%M-%S", &local_time);
    }
    const String filename =
        String("attachment; filename=\"maclock-backup-") +
        (timestamp[0] ? timestamp : "unknown-time") +
        ".zip\"";

    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader(
        "Content-Disposition", filename);
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/zip", "");
    stream_zip(server, entries);
    server.sendContent("");
    state_->events->endControlPanelNetworkTransfer();
}

void ConfigurationArchive::receiveUpload(
    WebServer &server)
{
    if (!state_ || !state_->events)
        return;
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        state_->upload_error.clear();
        state_->upload_finished = false;
        state_->events->beginControlPanelNetworkTransfer();
        state_->transfer_active = true;
        state_->upload_started = state_->restore.begin();
        if (!state_->upload_started)
            state_->upload_error =
                state_->restore.error();
        return;
    }
    if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (state_->upload_started &&
            !state_->restore.feed(
                upload.buf, upload.currentSize))
        {
            state_->upload_error =
                state_->restore.error();
            state_->upload_started = false;
        }
        return;
    }
    if (upload.status == UPLOAD_FILE_ABORTED)
    {
        state_->restore.abort();
        state_->upload_started = false;
        state_->upload_finished = true;
        state_->upload_error =
            "Backup upload was cancelled";
        return;
    }
    if (upload.status == UPLOAD_FILE_END)
    {
        if (state_->upload_started &&
            !state_->restore.finish())
        {
            state_->upload_error =
                state_->restore.error();
        }
        state_->upload_started = false;
        state_->upload_finished = true;
    }
}

void ConfigurationArchive::finishUpload(
    WebServer &server)
{
    std::vector<String> warnings;
    bool network_changed = false;
    if (!state_ || !state_->events)
    {
        send_result(
            server, false,
            "Control service is unavailable",
            warnings, false, 503);
        return;
    }
    if (!state_->upload_finished ||
        state_->upload_error.length())
    {
        const String message =
            state_->upload_error.length()
                ? state_->upload_error
                : String("No backup was uploaded");
        state_->restore.abort();
        if (state_->transfer_active)
        {
            state_->events->endControlPanelNetworkTransfer();
            state_->transfer_active = false;
        }
        send_result(
            server, false, message.c_str(),
            warnings, false, 400);
        return;
    }

    ControlPanelConfiguration configuration;
    String error;
    if (!deserialize_configuration(
            state_->restore.configuration(),
            configuration, error) ||
        !replace_restored_files())
    {
        if (!error.length())
            error = "Could not install restored files";
        state_->restore.abort();
        if (state_->transfer_active)
        {
            state_->events->endControlPanelNetworkTransfer();
            state_->transfer_active = false;
        }
        send_result(
            server, false, error.c_str(),
            warnings, false, 400);
        return;
    }

    resolve_restored_sounds(configuration, warnings);
    const String previous_ssid =
        state_->events->controlPanelConfiguration()
            .wifi.ssid;
    if (!state_->events->applyControlConfiguration(
            configuration, network_changed))
    {
        state_->restore.abort();
        if (state_->transfer_active)
        {
            state_->events->endControlPanelNetworkTransfer();
            state_->transfer_active = false;
        }
        send_result(
            server, false,
            "Configuration could not be restored",
            warnings, false, 500);
        return;
    }
    if (previous_ssid != configuration.wifi.ssid)
    {
        warnings.push_back(
            "The Wi-Fi password was not restored; "
            "Maclock kept its current password");
    }

    state_->restore.abort();
    if (state_->transfer_active)
    {
        state_->events->endControlPanelNetworkTransfer();
        state_->transfer_active = false;
    }
    send_result(
        server, true,
        network_changed
            ? "Backup restored; reconnecting to Wi-Fi"
            : "Backup restored",
        warnings, network_changed, 200);
}
