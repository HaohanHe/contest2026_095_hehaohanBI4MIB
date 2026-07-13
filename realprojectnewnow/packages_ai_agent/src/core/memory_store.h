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

#ifdef __cplusplus
extern "C" {
#endif

int memory_store_init(void);
int memory_read_long_term(char *buf, size_t size);
int memory_write_long_term(const char *content);
int memory_append_today(const char *note);
int memory_read_recent(char *buf, size_t size, int days);

#ifdef __cplusplus
}
#endif
