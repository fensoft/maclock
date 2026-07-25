#pragma once

#include <stddef.h>
#include <stdint.h>

size_t audio_write_stereo_frames(const int16_t *frames, size_t frame_count);
