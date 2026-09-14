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

#include "tools/tool_control.h"
#include "agent_compat.h"

#include <string.h>
#include <syslog.h>

#include "cJSON.h"

#ifdef CONFIG_VIBRATOR
#include <vibrator_api.h>
#endif

int tool_vibrate_execute(const char *input_json, char *output, size_t output_size)
{
#ifdef CONFIG_VIBRATOR
    int duration_ms = 200;   /* default */
    int amplitude   = 128;   /* default mid-strength */

    cJSON *root = cJSON_Parse(input_json);
    if (root) {
        cJSON *d = cJSON_GetObjectItem(root, "duration_ms");
        cJSON *a = cJSON_GetObjectItem(root, "amplitude");
        if (d && cJSON_IsNumber(d)) duration_ms = (int)d->valuedouble;
        if (a && cJSON_IsNumber(a)) amplitude   = (int)a->valuedouble;
        cJSON_Delete(root);
    }

    /* Clamp values */
    if (duration_ms < 1)   duration_ms = 1;
    if (duration_ms > 5000) duration_ms = 5000;
    if (amplitude < 1)     amplitude = 1;
    if (amplitude > 255)   amplitude = 255;

    int ret = vibrator_play_oneshot((uint32_t)duration_ms, (uint8_t)amplitude);
    if (ret < 0) {
        snprintf(output, output_size, "{\"error\":\"vibrator_play_oneshot failed: %d\"}", ret);
        return ERROR;
    }

    snprintf(output, output_size,
             "{\"ok\":true,\"duration_ms\":%d,\"amplitude\":%d}",
             duration_ms, amplitude);
    return OK;
#else
    (void)input_json;
    snprintf(output, output_size, "{\"error\":\"vibrator not enabled\"}");
    return ERROR;
#endif
}
