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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Tool: list members of a Feishu group chat.
 * Input: {"chat_id": "oc_xxx"}
 * Output: JSON array of {name, open_id}
 */
int tool_feishu_chat_members_execute(const char* input_json, char* output,
    size_t output_size);

/**
 * Tool: send a message that @mentions specific users in a Feishu chat.
 * Input: {"chat_id": "oc_xxx", "open_id": "ou_xxx", "name": "nana", "text":
 * "hello"} The message will be: @nana hello
 */
int tool_feishu_send_mention_execute(const char* input_json, char* output,
    size_t output_size);

#ifdef __cplusplus
}
#endif
