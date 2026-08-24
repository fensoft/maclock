#include "alarm_ui_internal.h"

#include "audio_volume.h"

#include <stdio.h>

namespace
{
constexpr uint32_t kAlarmStorageMagic = 0x414C524D;
constexpr uint8_t kAlarmStorageVersion = 3;
constexpr uint8_t kAlarmStorageVersionV2 = 2;
constexpr uint8_t kAlarmStorageVersionV1 = 1;
constexpr uint8_t kAllWeekdays = 0x7F;
constexpr size_t kLegacyAlarmSoundCount = 3;

struct LegacyAlarmConfig
{
    uint8_t enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t weekdays;
    uint8_t sound;
    uint8_t volume;
};
static_assert(sizeof(LegacyAlarmConfig) == 6, "Legacy alarm storage layout changed");

struct AlarmStorage
{
    uint32_t magic;
    uint8_t version;
    AlarmSettings alarms[kAlarmCount];
};

struct LegacyAlarmStorage
{
    uint32_t magic;
    uint8_t version;
    LegacyAlarmConfig alarms[kAlarmCount];
};

const char *const kLegacySoundPaths[kLegacyAlarmSoundCount] = {
    "/quack.mp3", "/startup.mp3", "/floppy.mp3"};

bool legacy_alarm_config_is_valid(const LegacyAlarmConfig &alarm, bool version_one)
{
    return alarm.enabled <= 1 && alarm.hour < 24 && alarm.minute < 60 &&
           (alarm.weekdays & ~kAllWeekdays) == 0 && alarm.sound < kLegacyAlarmSoundCount &&
           alarm.volume < (version_one ? 4 : kAudioVolumeLevelCount);
}

void migrate_legacy_alarm(AlarmSettings &target, const LegacyAlarmConfig &source, bool version_one)
{
    target = AlarmSettings();
    target.enabled = source.enabled;
    target.hour = source.hour;
    target.minute = source.minute;
    target.weekdays = source.weekdays;
    target.sound = source.sound;
    target.volume = version_one ? audio_volume_legacy_index(source.volume) : source.volume;
}

void set_defaults()
{
    memset(g_alarms, 0, sizeof(g_alarms));
    for (size_t i = 0; i < kAlarmCount; ++i)
    {
        g_alarms[i].hour = static_cast<uint8_t>(7 + i);
        g_alarms[i].weekdays = kAllWeekdays;
        g_alarms[i].sound = 0;
        g_alarms[i].volume = kDefaultAudioVolumeIndex;
        strlcpy(g_alarm_sound_paths[i], "/quack.mp3", SOUND_SELECTOR_PATH_MAX);
        g_last_trigger_minute[i] = UINT32_MAX;
    }
}

void load_sound_paths(Preferences &preferences)
{
    for (size_t i = 0; i < kAlarmCount; ++i)
    {
        char key[20];
        snprintf(key, sizeof(key), "alarm_sound_%u", static_cast<unsigned>(i));
        const uint8_t legacy_sound = g_alarms[i].sound < kLegacyAlarmSoundCount ? g_alarms[i].sound : 0;
        const String saved = preferences.getString(key, kLegacySoundPaths[legacy_sound]);
        strlcpy(g_alarm_sound_paths[i], saved.c_str(), SOUND_SELECTOR_PATH_MAX);
    }
}
}

bool alarm_config_is_valid(const AlarmSettings &alarm)
{
    return alarm.enabled <= 1 && alarm.hour < 24 && alarm.minute < 60 &&
           (alarm.weekdays & ~kAllWeekdays) == 0 && alarm.sound < kLegacyAlarmSoundCount &&
           alarm.volume < kAudioVolumeLevelCount && alarm.one_time <= 1 &&
           alarm.gradual_volume <= 1 && alarm.sunrise <= 1 &&
           memchr(alarm.label, '\0', sizeof(alarm.label)) != nullptr;
}

void alarm_storage_save()
{
    if (!g_preferences)
        return;
    AlarmStorage storage = {};
    storage.magic = kAlarmStorageMagic;
    storage.version = kAlarmStorageVersion;
    memcpy(storage.alarms, g_alarms, sizeof(g_alarms));
    g_preferences->putBytes("alarms_v1", &storage, sizeof(storage));
    for (size_t i = 0; i < kAlarmCount; ++i)
    {
        char key[20];
        snprintf(key, sizeof(key), "alarm_sound_%u", static_cast<unsigned>(i));
        g_preferences->putString(key, g_alarm_sound_paths[i]);
    }
}

void alarm_storage_begin(AlarmService &service, Preferences &preferences)
{
    g_preferences = &preferences;
    set_defaults();
    g_snooze_alarm = -1;
    g_snooze_at = 0;
    bool storage_valid = false;
    bool migrated = false;
    const size_t storage_length = preferences.getBytesLength("alarms_v1");
    if (storage_length == sizeof(AlarmStorage))
    {
        AlarmStorage storage = {};
        storage_valid = preferences.getBytes("alarms_v1", &storage, sizeof(storage)) == sizeof(storage) &&
                        storage.magic == kAlarmStorageMagic && storage.version == kAlarmStorageVersion;
        for (size_t i = 0; storage_valid && i < kAlarmCount; ++i)
            storage_valid = alarm_config_is_valid(storage.alarms[i]);
        if (storage_valid)
            memcpy(g_alarms, storage.alarms, sizeof(g_alarms));
    }
    else if (storage_length == sizeof(LegacyAlarmStorage))
    {
        LegacyAlarmStorage storage = {};
        storage_valid = preferences.getBytes("alarms_v1", &storage, sizeof(storage)) == sizeof(storage) &&
                        storage.magic == kAlarmStorageMagic &&
                        (storage.version == kAlarmStorageVersionV1 || storage.version == kAlarmStorageVersionV2);
        const bool version_one = storage.version == kAlarmStorageVersionV1;
        for (size_t i = 0; storage_valid && i < kAlarmCount; ++i)
            storage_valid = legacy_alarm_config_is_valid(storage.alarms[i], version_one);
        if (storage_valid)
        {
            for (size_t i = 0; i < kAlarmCount; ++i)
                migrate_legacy_alarm(g_alarms[i], storage.alarms[i], version_one);
            migrated = true;
        }
    }
    load_sound_paths(preferences);
    if (storage_valid && migrated)
        alarm_storage_save();
    (void)service;
}
