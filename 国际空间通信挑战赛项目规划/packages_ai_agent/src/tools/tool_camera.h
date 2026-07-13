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

#include "agent_compat.h"
#include <stddef.h>

/**
 * camera_capture tool: Capture a photo from the device camera via V4L2,
 * then send the JPEG to a Vision LLM for analysis.
 *
 * Input JSON:
 *   {"prompt": "describe what you see"}       — optional analysis prompt
 *   {"resolution": "low"}                     — optional: "low" (320x180) or "high" (1280x720)
 *
 * Output JSON:
 *   {"analysis": "I see a ..."}               — on success
 *   {"error": "..."}                          — on failure
 */
int tool_camera_capture_execute(const char *input_json,
                                char *output, size_t output_size);
