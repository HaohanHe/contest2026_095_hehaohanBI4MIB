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

#include "tools/tool_vision.h"
#include "llm/llm_proxy.h"
#include "agent_compat.h"
#include "agent_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"
#include "mbedtls/base64.h"

static const char* TAG = "vision";

static int auto_capture(const char* path)
{
#ifdef CONFIG_SYSTEM_POPEN
    /* Validate path: reject shell metacharacters to prevent injection */
    for (const char* p = path; *p; p++) {
        if (*p == ';' || *p == '|' || *p == '&' || *p == '`'
            || *p == '$' || *p == '(' || *p == ')' || *p == '\n') {
            syslog(LOG_ERR, "[%s] Rejected path with shell metachar\n", TAG);
            return ERROR;
        }
    }
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "fbcapture '%s'", path);
    syslog(LOG_INFO, "[%s] Auto-capture: %s\n", TAG, cmd);
    FILE* pp = popen(cmd, "r");
    if (!pp) {
        syslog(LOG_ERR, "[%s] popen fbcapture failed\n", TAG);
        return ERROR;
    }
    char buf[128];
    while (fgets(buf, sizeof(buf), pp))
        ;
    int rc = pclose(pp);
    syslog(LOG_INFO, "[%s] fbcapture exit=%d\n", TAG, rc);
    return (rc == 0) ? OK : ERROR;
#else
    (void)path;
    syslog(LOG_WARNING, "[%s] popen not available, cannot auto-capture\n", TAG);
    return ERROR;
#endif
}

int tool_analyze_image_execute(const char* input_json, char* output, size_t output_size)
{
    const char* image_path = AGENT_CAPTURE_PATH;
    const char* prompt = NULL;
    bool need_capture = true;

    /* Parse input: {"image_path": "...", "prompt": "..."} */
    cJSON* root = cJSON_Parse(input_json);
    if (root) {
        cJSON* p = cJSON_GetObjectItem(root, "image_path");
        if (p && cJSON_IsString(p) && p->valuestring[0]) {
            image_path = p->valuestring;
            need_capture = false;
        }
        cJSON* q = cJSON_GetObjectItem(root, "prompt");
        if (q && cJSON_IsString(q) && q->valuestring[0])
            prompt = q->valuestring;
    }

    /* Auto-capture screen if using default path */
    if (need_capture) {
        if (auto_capture(image_path) != OK) {
            snprintf(output, output_size,
                "{\"error\":\"fbcapture failed, is display active?\"}");
            cJSON_Delete(root);
            return ERROR;
        }
    }

    /* Read image file */
    FILE* fp = fopen(image_path, "rb");
    if (!fp) {
        snprintf(output, output_size, "{\"error\":\"Cannot open image: %s\"}", image_path);
        cJSON_Delete(root);
        return ERROR;
    }

    struct stat st;
    if (stat(image_path, &st) != 0 || st.st_size <= 0) {
        fclose(fp);
        snprintf(output, output_size, "{\"error\":\"Cannot stat image: %s\"}", image_path);
        cJSON_Delete(root);
        return ERROR;
    }

    if ((size_t)st.st_size > AGENT_VISION_MAX_IMAGE_SIZE) {
        fclose(fp);
        snprintf(output, output_size,
            "{\"error\":\"Image too large (%ld bytes, max %d)\"}",
            (long)st.st_size, AGENT_VISION_MAX_IMAGE_SIZE);
        cJSON_Delete(root);
        return ERROR;
    }

    unsigned char* raw = malloc((size_t)st.st_size);
    if (!raw) {
        fclose(fp);
        snprintf(output, output_size, "{\"error\":\"OOM reading image\"}");
        cJSON_Delete(root);
        return ERROR;
    }

    size_t nread = fread(raw, 1, (size_t)st.st_size, fp);
    fclose(fp);

    if (nread != (size_t)st.st_size) {
        free(raw);
        snprintf(output, output_size, "{\"error\":\"Short read on image\"}");
        cJSON_Delete(root);
        return ERROR;
    }

    /* Detect image format from magic bytes */
    const char* mime_type = "image/jpeg";
    if (nread >= 4 && raw[0] == 0x89 && raw[1] == 'P'
        && raw[2] == 'N' && raw[3] == 'G')
        mime_type = "image/png";
    else if (nread >= 4 && !memcmp(raw, "GIF8", 4))
        mime_type = "image/gif";
    else if (nread >= 12 && !memcmp(raw, "RIFF", 4) && !memcmp(raw + 8, "WEBP", 4))
        mime_type = "image/webp";

    syslog(LOG_INFO, "[%s] Image %s: %ld bytes (%s)\n",
        TAG, image_path, (long)nread, mime_type);

    size_t b64_len = ((nread + 2) / 3) * 4;
    if (b64_len > AGENT_VISION_MAX_B64_SIZE) {
        free(raw);
        snprintf(output, output_size, "{\"error\":\"Base64 too large\"}");
        cJSON_Delete(root);
        return ERROR;
    }

    char* b64 = malloc(b64_len + 1);
    if (!b64) {
        free(raw);
        snprintf(output, output_size, "{\"error\":\"OOM for base64\"}");
        cJSON_Delete(root);
        return ERROR;
    }

    size_t written = 0;
    mbedtls_base64_encode((unsigned char*)b64, b64_len, &written, raw, nread);
    b64[written] = '\0';

    syslog(LOG_INFO, "[%s] Image %s: %ld bytes -> %d b64 chars (%s)\n",
        TAG, image_path, (long)nread, (int)written, mime_type);

    /* Call vision LLM */
    char* resp_buf = calloc(1, output_size);
    if (!resp_buf) {
        free(raw);
        free(b64);
        snprintf(output, output_size, "{\"error\":\"OOM for response\"}");
        cJSON_Delete(root);
        return ERROR;
    }

    int ret = llm_chat_vision_raw(prompt, raw, nread, mime_type,
        resp_buf, output_size);
    free(raw);
    free(b64);
    cJSON_Delete(root);

    if (ret != OK) {
        snprintf(output, output_size, "{\"error\":\"%s\"}", resp_buf);
        free(resp_buf);
        return ret;
    }

    /* Return as JSON */
    cJSON* result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "analysis", resp_buf);
    free(resp_buf);

    char* json_str = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);

    if (json_str) {
        strncpy(output, json_str, output_size - 1);
        output[output_size - 1] = '\0';
        syslog(LOG_INFO, "[%s] Analysis complete: %d bytes\n", TAG, (int)strlen(output));
        free(json_str);
    }

    return OK;
}
