#pragma once

#include <stddef.h>
#include <stdint.h>

static constexpr uint8_t kAudioVolumeLevels[] = {
    10, 20, 40, 60, 80, 100};
static constexpr size_t kAudioVolumeLevelCount =
    sizeof(kAudioVolumeLevels) / sizeof(kAudioVolumeLevels[0]);
static constexpr uint8_t kDefaultAudioVolumeIndex = 4;

inline bool audio_volume_is_level(uint8_t volume)
{
    for (size_t i = 0; i < kAudioVolumeLevelCount; ++i)
    {
        if (kAudioVolumeLevels[i] == volume)
            return true;
    }
    return false;
}

inline uint8_t audio_volume_from_index(
    uint8_t index,
    uint8_t fallback_index = kDefaultAudioVolumeIndex)
{
    if (index >= kAudioVolumeLevelCount)
        index = fallback_index;
    if (index >= kAudioVolumeLevelCount)
        index = kDefaultAudioVolumeIndex;
    return kAudioVolumeLevels[index];
}

inline uint8_t audio_volume_nearest_level(uint8_t volume)
{
    size_t best = 0;
    uint16_t best_distance = 256;
    for (size_t i = 0; i < kAudioVolumeLevelCount; ++i)
    {
        const uint16_t distance =
            volume > kAudioVolumeLevels[i]
                ? volume - kAudioVolumeLevels[i]
                : kAudioVolumeLevels[i] - volume;
        if (distance < best_distance)
        {
            best = i;
            best_distance = distance;
        }
    }
    return kAudioVolumeLevels[best];
}

inline uint8_t audio_volume_legacy_index(uint8_t index)
{
    static constexpr uint8_t kLegacyToCurrent[] = {
        1, // 25% -> 20%
        2, // 50% -> 40%
        4, // 75% -> 80%
        5  // 100% -> 100%
    };
    return index < sizeof(kLegacyToCurrent)
               ? kLegacyToCurrent[index]
               : kDefaultAudioVolumeIndex;
}

inline uint8_t audio_volume_codec_level(uint8_t volume)
{
    // ES8311 volume is a linear DAC-register control, not a perceptual
    // percentage. Keep the selectable range between roughly -12 dB and
    // 0 dB, and cap the loudest setting below the clipping-prone register
    // range reached by the old direct 100% mapping.
    static constexpr uint8_t kCodecLevels[] = {
        57, 61, 65, 68, 72, 75};
    const uint8_t normalized = audio_volume_nearest_level(volume);
    for (size_t i = 0; i < kAudioVolumeLevelCount; ++i)
    {
        if (kAudioVolumeLevels[i] == normalized)
            return kCodecLevels[i];
    }
    return kCodecLevels[kDefaultAudioVolumeIndex];
}
