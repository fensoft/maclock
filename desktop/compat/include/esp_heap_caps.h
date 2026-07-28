#pragma once

#include <stddef.h>

#define MALLOC_CAP_DEFAULT 0
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_INTERNAL 4

void *heap_caps_malloc(size_t size, unsigned capabilities);
void *heap_caps_calloc(
    size_t count, size_t size, unsigned capabilities);
void *heap_caps_aligned_alloc(
    size_t alignment, size_t size, unsigned capabilities);
void heap_caps_free(void *memory);
void heap_caps_malloc_extmem_enable(size_t limit);
