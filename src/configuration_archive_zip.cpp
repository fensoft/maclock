#ifdef MACLOCK_CONFIGURATION_ARCHIVE_COMBINED_SOURCE
#include "configuration_archive.h"
#include "configuration_archive_internal.h"

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
static constexpr uint8_t kArchiveVersion = 2;
static constexpr char kConfigurationEntry[] =
    "configuration.json";
static constexpr char kDownloadedPrefix[] =
    "downloaded/";
static constexpr char kFloppyPrefix[] =
    "floppies/";
static constexpr char kLoadingPrefix[] =
    "loading/";
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
static constexpr char kRestoreLoading[] =
    "/.maclock-restore/loading";
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

#endif
