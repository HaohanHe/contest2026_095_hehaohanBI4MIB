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

/*
 * This file contains code derived from MimiClaw (https://github.com/memovai/mimiclaw)
 * Copyright (c) 2026 Ziboyan Wang, licensed under the MIT License.
 * See NOTICE file for the original MIT License terms.
 */

#pragma once

#include "agent_compat.h"
#include <stddef.h>

int tool_web_search_init(void);

/* web_search tool: uses SerpAPI (primary) or Exa (fallback) or Tavily (fallback) */
int tool_web_search_execute(const char *input_json, char *output, size_t output_size);

/* news_search tool: uses NewsAPI */
int tool_news_search_execute(const char *input_json, char *output, size_t output_size);

/* get_weather tool: uses wttr.in (no API key required) */
int tool_get_weather_execute(const char *input_json, char *output, size_t output_size);

/* CLI setters */
int tool_web_search_set_key(const char *api_key);   /* alias → serp key */
int tool_web_search_set_serp_key(const char *key);
int tool_web_search_set_exa_key(const char *key);
int tool_web_search_set_tavily_key(const char *key);
int tool_web_search_set_news_key(const char *key);
