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

#include "tools/tool_amap.h"
#include "infra/vela_tls.h"
#include "cJSON.h"
#include "agent_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "amap";
static char s_api_key[64] = "";

#define AMAP_HOST "restapi.amap.com"
#define AMAP_PORT "443"
#define AMAP_RESP_SIZE 4096

int tool_amap_set_key(const char *api_key)
{
    if (!api_key || api_key[0] == '\0') return ERROR;
    strncpy(s_api_key, api_key, sizeof(s_api_key) - 1);
    s_api_key[sizeof(s_api_key) - 1] = '\0';
    syslog(LOG_INFO, "[%s] API key set\n", TAG);
    return OK;
}

/* ── Helper: GET request to AMap REST API ─────────────── */

static int __attribute__((unused)) amap_get(const char *path, char *resp, size_t resp_size)
{
    if (s_api_key[0] == '\0') {
        snprintf(resp, resp_size, "Error: AMap API key not set. Use set_amap_key <key>");
        return ERROR;
    }

    int status = vela_https_get(AMAP_HOST, AMAP_PORT, path, resp, resp_size);
    if (status < 200 || status >= 300) {
        syslog(LOG_ERR, "[%s] HTTP %d for %s\n", TAG, status, path);
        snprintf(resp, resp_size, "Error: AMap API returned HTTP %d", status);
        return ERROR;
    }

    return OK;
}

/* ── Helper: extract useful fields from AMap JSON ─────── */

static void __attribute__((unused)) format_error(const char *raw, char *output, size_t size)
{
    cJSON *root = cJSON_Parse(raw);
    if (!root) {
        snprintf(output, size, "Error: invalid JSON from AMap");
        return;
    }

    cJSON *status = cJSON_GetObjectItem(root, "status");
    cJSON *info = cJSON_GetObjectItem(root, "info");

    if (status && cJSON_IsString(status) && strcmp(status->valuestring, "0") == 0) {
        snprintf(output, size, "AMap error: %s",
                 (info && cJSON_IsString(info)) ? info->valuestring : "unknown");
    }

    cJSON_Delete(root);
}
