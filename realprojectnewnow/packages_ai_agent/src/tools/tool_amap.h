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

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set AMap API key. Must be called before using any amap tools.
 */
int tool_amap_set_key(const char *api_key);

/* Tool execute functions (registered in tool_registry) */
int tool_amap_weather(const char *input_json, char *output, size_t output_size);
int tool_amap_geocode(const char *input_json, char *output, size_t output_size);
int tool_amap_search(const char *input_json, char *output, size_t output_size);
int tool_amap_ip_location(const char *input_json, char *output, size_t output_size);

#ifdef __cplusplus
}
#endif
