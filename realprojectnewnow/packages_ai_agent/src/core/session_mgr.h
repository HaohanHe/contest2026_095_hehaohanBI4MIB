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

int session_mgr_init(void);
int session_append(const char *chat_id, const char *role, const char *content);
int session_get_history_json(const char *chat_id, char *buf, size_t size, int max_msgs);
int session_clear(const char *chat_id);
int session_clear_all(void);
void      session_list(void);

#ifdef __cplusplus
}
#endif
