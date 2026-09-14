/*
 * Copyright (C) 2026 Xiaomi Corporation
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

#include <stddef.h>

/**
 * Play a music URL. If already playing, stops the current track first.
 * Input: {"url":"...", "autostart":true, "start_position_ms":0}
 */

int tool_music_play_execute(const char* input_json,
    char* output, size_t output_size);

/**
 * Pause the current playback.
 * Input: {}
 */

int tool_music_pause_execute(const char* input_json,
    char* output, size_t output_size);

/**
 * Resume paused playback.
 * Input: {}
 */

int tool_music_resume_execute(const char* input_json,
    char* output, size_t output_size);

/**
 * Stop playback and release resources.
 * Input: {}
 */

int tool_music_stop_execute(const char* input_json,
    char* output, size_t output_size);

/**
 * Seek to a position in the current track.
 * Input: {"position_ms": 90000}
 */

int tool_music_seek_execute(const char* input_json,
    char* output, size_t output_size);

/**
 * Set music stream volume (0-100).
 * Input: {"volume": 80}
 */

int tool_music_set_volume_execute(const char* input_json,
    char* output, size_t output_size);

/**
 * Get current playback status.
 * Input: {}
 */

int tool_music_status_execute(const char* input_json,
    char* output, size_t output_size);

/**
 * Module cleanup — stop playback, release all resources.
 * Must be called on program exit.
 */

void tool_media_cleanup(void);

/**
 * Search songs by keyword via Netease Cloud Music API.
 * Input: {"keyword":"..."}
 */

int tool_music_search_execute(const char* input_json,
    char* output, size_t output_size);
