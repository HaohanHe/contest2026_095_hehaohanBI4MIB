/****************************************************************************
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
 ****************************************************************************/

/**
 * daynote_asr.c - SiliconFlow ASR backend
 *
 * Implements voice_asr_ops_t for SiliconFlow API (OpenAI-compatible):
 *   POST https://api.siliconflow.cn/v1/audio/transcriptions
 *   multipart/form-data with WAV file + model
 *   Response: {"text": "..."}
 *
 * Also supports any OpenAI-compatible ASR endpoint via config_store:
 *   asr_host, asr_path, asr_model, asr_api_key
 */

#include "voice/voice_asr.h"
#include "infra/config_store.h"
#include "infra/vela_tls.h"
#include "daynote_asr.h"

#include <netutils/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#define ASR_TAG "daynote_asr"

#define ASR_DEFAULT_HOST  "api.siliconflow.cn"
#define ASR_DEFAULT_PORT  "443"
#define ASR_DEFAULT_PATH  "/v1/audio/transcriptions"
#define ASR_DEFAULT_MODEL "FunAudioLLM/SenseVoiceSmall"

#define ASR_WAV_HDR_SIZE  44
#define ASR_MULTIPART_BOUNDARY "----daynoteasr7d2c"
#define ASR_RESP_BUF_SIZE  4096
#define ASR_MAX_AUDIO_SIZE (960 * 1024)  /* 30s at 16kHz/16bit/mono */

static char s_api_key[128] = {0};
static char s_host[128] = {0};
static char s_path[128] = {0};
static char s_model[64] = {0};

/* Write WAV header for 16kHz mono 16-bit PCM */
static void write_wav_header(uint8_t* buf, uint32_t pcm_len)
{
    uint32_t file_size = pcm_len + ASR_WAV_HDR_SIZE - 8;
    uint32_t byte_rate = 16000 * 1 * 2;  /* sample_rate * channels * bits/8 */

    memcpy(buf, "RIFF", 4);
    buf[4] = file_size & 0xFF; buf[5] = (file_size >> 8) & 0xFF;
    buf[6] = (file_size >> 16) & 0xFF; buf[7] = (file_size >> 24) & 0xFF;
    memcpy(buf + 8, "WAVE", 4);
    memcpy(buf + 12, "fmt ", 4);
    buf[16] = 16; buf[17] = 0; buf[18] = 0; buf[19] = 0;  /* chunk size */
    buf[20] = 1; buf[21] = 0;                                /* PCM format */
    buf[22] = 1; buf[23] = 0;                                /* mono */
    buf[24] = 0x80; buf[25] = 0x3E; buf[26] = 0; buf[27] = 0; /* 16000 */
    buf[28] = byte_rate & 0xFF; buf[29] = (byte_rate >> 8) & 0xFF;
    buf[30] = (byte_rate >> 16) & 0xFF; buf[31] = (byte_rate >> 24) & 0xFF;
    buf[32] = 2; buf[33] = 0;                                /* block align */
    buf[34] = 16; buf[35] = 0;                               /* bits per sample */
    memcpy(buf + 36, "data", 4);
    buf[40] = pcm_len & 0xFF; buf[41] = (pcm_len >> 8) & 0xFF;
    buf[42] = (pcm_len >> 16) & 0xFF; buf[43] = (pcm_len >> 24) & 0xFF;
}

static int load_config(void)
{
    char buf[128];

    if (claw_config_get("asr_api_key", buf, sizeof(buf)) > 0) {
        strncpy(s_api_key, buf, sizeof(s_api_key) - 1);
    }
    if (claw_config_get("asr_host", buf, sizeof(buf)) > 0) {
        strncpy(s_host, buf, sizeof(s_host) - 1);
    } else {
        strncpy(s_host, ASR_DEFAULT_HOST, sizeof(s_host) - 1);
    }
    if (claw_config_get("asr_path", buf, sizeof(buf)) > 0) {
        strncpy(s_path, buf, sizeof(s_path) - 1);
    } else {
        strncpy(s_path, ASR_DEFAULT_PATH, sizeof(s_path) - 1);
    }
    if (claw_config_get("asr_model", buf, sizeof(buf)) > 0) {
        strncpy(s_model, buf, sizeof(s_model) - 1);
    } else {
        strncpy(s_model, ASR_DEFAULT_MODEL, sizeof(s_model) - 1);
    }

    if (s_api_key[0] == '\0') {
        syslog(LOG_WARNING, "[%s] no API key configured\n", ASR_TAG);
        return -1;
    }
    return 0;
}

static int sf_asr_init(void)
{
    syslog(LOG_INFO, "[%s] init (SiliconFlow ASR)\n", ASR_TAG);
    return load_config();
}

static int sf_asr_recognize(const unsigned char* pcm_data, size_t pcm_len,
                            char* text_out, size_t text_cap)
{
    uint8_t* body_buf = NULL;
    char* resp_buf = NULL;
    char auth_hdr[256];
    char content_type[256];
    int body_len;
    int prefix_len;
    int file_header_len;
    int suffix_len;
    int ret = -1;
    int http_status;
    cJSON* root;
    cJSON* text_obj;
    const char* b = ASR_MULTIPART_BOUNDARY;

    if (!pcm_data || pcm_len == 0 || !text_out || text_cap == 0) {
        return -EINVAL;
    }

    if (pcm_len > ASR_MAX_AUDIO_SIZE) {
        syslog(LOG_WARNING, "[%s] audio too large (%zu bytes)\n",
            ASR_TAG, pcm_len);
        pcm_len = ASR_MAX_AUDIO_SIZE;
    }

    /* Reload config in case it was updated via BLE */
    load_config();
    if (s_api_key[0] == '\0') {
        return -EACCES;
    }

    /* Construct multipart/form-data body */
    prefix_len = snprintf(NULL, 0,
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
        "%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n",
        b, s_model, b);

    suffix_len = snprintf(NULL, 0, "\r\n--%s--\r\n", b);
    file_header_len = ASR_WAV_HDR_SIZE;
    body_len = prefix_len + file_header_len + (int)pcm_len + suffix_len;

    body_buf = malloc(body_len);
    if (!body_buf) return -ENOMEM;

    resp_buf = malloc(ASR_RESP_BUF_SIZE);
    if (!resp_buf) { free(body_buf); return -ENOMEM; }

    /* Write multipart prefix */
    snprintf((char*)body_buf, prefix_len + 1,
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
        "%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n",
        b, s_model, b);

    /* Write WAV header + PCM data */
    write_wav_header(body_buf + prefix_len, (uint32_t)pcm_len);
    memcpy(body_buf + prefix_len + file_header_len, pcm_data, pcm_len);

    /* Write multipart suffix */
    snprintf((char*)body_buf + prefix_len + file_header_len + pcm_len,
        suffix_len + 1, "\r\n--%s--\r\n", b);

    /* Build headers */
    snprintf(auth_hdr, sizeof(auth_hdr), "Bearer %s", s_api_key);
    snprintf(content_type, sizeof(content_type),
        "multipart/form-data; boundary=%s", b);

    vela_header_t hdrs[] = {
        { "Authorization", auth_hdr },
        { "Content-Type", content_type },
        { NULL, NULL }
    };

    syslog(LOG_INFO, "[%s] sending %d bytes to %s%s\n",
        ASR_TAG, body_len, s_host, s_path);

    http_status = vela_https_request(
        s_host, ASR_DEFAULT_PORT, "POST", s_path,
        hdrs, (const char*)body_buf, body_len,
        resp_buf, ASR_RESP_BUF_SIZE, NULL);

    free(body_buf);

    if (http_status != 200) {
        syslog(LOG_WARNING, "[%s] HTTP %d: %.200s\n",
            ASR_TAG, http_status, resp_buf);
        free(resp_buf);
        return -1;
    }

    /* Parse JSON response */
    root = cJSON_Parse(resp_buf);
    free(resp_buf);

    if (!root) {
        syslog(LOG_WARNING, "[%s] JSON parse failed\n", ASR_TAG);
        return -1;
    }

    text_obj = cJSON_GetObjectItem(root, "text");
    if (text_obj && cJSON_IsString(text_obj) && text_obj->valuestring) {
        strncpy(text_out, text_obj->valuestring, text_cap - 1);
        text_out[text_cap - 1] = '\0';
        ret = 0;
        syslog(LOG_INFO, "[%s] result: \"%s\"\n", ASR_TAG, text_out);
    } else {
        syslog(LOG_WARNING, "[%s] no text in response\n", ASR_TAG);
    }

    cJSON_Delete(root);
    return ret;
}

static void sf_asr_deinit(void)
{
    syslog(LOG_INFO, "[%s] deinit\n", ASR_TAG);
}

static const voice_asr_ops_t s_sf_asr_ops = {
    .name      = "siliconflow",
    .init      = sf_asr_init,
    .recognize = sf_asr_recognize,
    .deinit    = sf_asr_deinit,
};

int daynote_asr_register(void)
{
    return voice_asr_register(&s_sf_asr_ops);
}
