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

#ifndef TOOL_PROXYQUICKAPP_H
#define TOOL_PROXYQUICKAPP_H

#include <stddef.h>

int tool_launch_quickapp_execute(const char *input_json, char *output, size_t output_size);
int tool_exit_quickapp_execute(const char *input_json, char *output, size_t output_size);

#endif /* TOOL_PROXYQUICKAPP_H */
