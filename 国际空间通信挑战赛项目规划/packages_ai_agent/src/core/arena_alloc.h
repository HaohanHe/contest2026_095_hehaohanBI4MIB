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

#pragma once

/**
 * arena_alloc.h — Arena (bump) allocator for AI Agent.
 *
 * Groups all temporary allocations within a single Agent loop
 * iteration into one contiguous block. When the iteration ends,
 * the entire arena is freed at once — zero fragmentation.
 *
 * Usage:
 *   arena_t arena;
 *   arena_init(&arena, 4096);       // allocate backing buffer
 *   void *p1 = arena_alloc(&arena, 128);
 *   void *p2 = arena_alloc(&arena, 256);
 *   // ... use p1, p2 ...
 *   arena_reset(&arena);            // free all at once (zero cost)
 *   arena_destroy(&arena);          // release backing buffer
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char   *buf;       /* backing buffer (heap-allocated) */
    size_t  capacity;  /* total size of buf */
    size_t  used;      /* current allocation offset */
    int     alloc_count; /* number of allocations (for stats) */
} arena_t;

/**
 * Initialize arena with given capacity.
 * Returns 0 on success, -ENOMEM on failure.
 */
int arena_init(arena_t *a, size_t capacity);

/**
 * Allocate `size` bytes from the arena (bump pointer).
 * Returns aligned pointer, or NULL if arena is full.
 * Allocations are NOT individually freeable.
 */
void *arena_alloc(arena_t *a, size_t size);

/**
 * Reset arena — all previous allocations become invalid.
 * The backing buffer is retained for reuse. Zero cost.
 */
void arena_reset(arena_t *a);

/**
 * Destroy arena and free the backing buffer.
 */
void arena_destroy(arena_t *a);

/**
 * Query arena usage statistics.
 */
size_t arena_used(const arena_t *a);
size_t arena_remaining(const arena_t *a);

#ifdef __cplusplus
}
#endif
