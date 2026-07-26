#pragma once

#include <stddef.h>
#include <stdint.h>

static constexpr int kClockAudioDmaBufferCount = 8;
static constexpr int kClockAudioDmaBufferBytes = 8192;

size_t audio_write_stereo_frames(const int16_t *frames, size_t frame_count);
