/*
 * Copyright (C) 2025 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "core/arena_alloc.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

static const char *TAG = "arena";

/* Align all allocations to pointer size for safety */
#define ARENA_ALIGN sizeof(void *)

static inline size_t align_up(size_t n, size_t align)
{
    return (n + align - 1) & ~(align - 1);
}

int arena_init(arena_t *a, size_t capacity)
{
    if (!a || capacity == 0) {
        return -EINVAL;
    }

    a->buf = malloc(capacity);
    if (!a->buf) {
        syslog(LOG_ERR, "[%s] Failed to allocate %zu bytes\n",
            TAG, capacity);
        return -ENOMEM;
    }

    a->capacity = capacity;
    a->used = 0;
    a->alloc_count = 0;
    return 0;
}

void *arena_alloc(arena_t *a, size_t size)
{
    if (!a || !a->buf || size == 0) {
        return NULL;
    }

    size_t aligned = align_up(size, ARENA_ALIGN);

    if (a->used + aligned > a->capacity) {
        syslog(LOG_WARNING,
            "[%s] Arena full: need %zu, have %zu/%zu\n",
            TAG, aligned, a->capacity - a->used, a->capacity);
        return NULL;
    }

    void *ptr = a->buf + a->used;
    a->used += aligned;
    a->alloc_count++;
    return ptr;
}

void arena_reset(arena_t *a)
{
    if (!a) {
        return;
    }

    if (a->alloc_count > 0) {
        syslog(LOG_DEBUG,
            "[%s] Reset: %d allocs, %zu/%zu bytes used\n",
            TAG, a->alloc_count, a->used, a->capacity);
    }

    a->used = 0;
    a->alloc_count = 0;
}

void arena_destroy(arena_t *a)
{
    if (!a) {
        return;
    }

    free(a->buf);
    a->buf = NULL;
    a->capacity = 0;
    a->used = 0;
    a->alloc_count = 0;
}

size_t arena_used(const arena_t *a)
{
    return a ? a->used : 0;
}

size_t arena_remaining(const arena_t *a)
{
    return a ? a->capacity - a->used : 0;
}
