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

#include "channels/feishu_internal.h"
#include "infra/config_store.h"
#include "cJSON.h"

#include <fcntl.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char* TAG = "feishu";

/* ── Shared state definitions (extern'd in feishu_internal.h) ──── */

char s_app_id[64] = AGENT_SECRET_FEISHU_APP_ID;
char s_app_secret[64] = AGENT_SECRET_FEISHU_APP_SECRET;
char s_access_token[512] = { 0 };
char s_user_token[512] = { 0 };
char s_bot_open_id[64] = { 0 };
int64_t s_token_expire_ts = 0;
feishu_ws_t s_ws;
mbedtls_ctr_drbg_context s_ctr_drbg;
bool s_rng_ready = false;

static sem_t s_cred_sem;

/* ── Simple entropy function (works on NuttX/QEMU) ─────────────── */

static int simple_entropy_func(void* data, unsigned char* output, size_t len)
{
    (void)data;
    if (agent_secure_random(output, len) == 0) {
        return 0;
    }
    syslog(LOG_ERR, "[feishu] CRITICAL: No secure entropy source available\n");
    return -1;  /* Generic error - TLS handshake will fail safely */
}

/* ── Background polling task ─────────────────────────────────── */

static void* feishu_poll_task(void* arg)
{
    (void)arg;
    syslog(LOG_INFO, "[%s] Feishu polling task started\n", TAG);

    const size_t frame_buf_size = 65536;
    uint8_t* frame_buf = malloc(frame_buf_size);

    if (!frame_buf) {
        syslog(LOG_ERR, "[%s] OOM for frame_buf\n", TAG);
        return NULL;
    }

    char* ws_host = malloc(128);
    char* ws_path = malloc(768);

    if (!ws_host || !ws_path) {
        syslog(LOG_ERR, "[%s] OOM for ws_host/ws_path\n", TAG);
        free(frame_buf);
        free(ws_host);
        free(ws_path);
        return NULL;
    }

    int32_t svc_id = 0;

    while (1) {
        /* Credentials check */
        if (s_app_id[0] == '\0' || s_app_secret[0] == '\0') {
            static int s_no_cred_logged = 0;
            if (!s_no_cred_logged) {
                syslog(LOG_WARNING,
                    "[%s] No app_id/app_secret. Use: "
                    "set_feishu_app <id> <secret>\n", TAG);
                s_no_cred_logged = 1;
            }
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 60;
            sem_timedwait(&s_cred_sem, &ts);
            continue;
        }

        /* Refresh access token */
        if (feishu_token_expired()) {
            if (feishu_get_app_token() != OK) {
                syslog(LOG_WARNING, "[%s] Token refresh failed; retry in 30s\n", TAG);
                sleep(30);
                continue;
            }
        }

        /* Get WS endpoint */
        memset(ws_host, 0, 128);
        memset(ws_path, 0, 768);
        if (feishu_create_connection(ws_host, 128, ws_path, 768) != OK) {
            syslog(LOG_WARNING, "[%s] create_connection failed; retry in 10s\n", TAG);
            sleep(10);
            continue;
        }

        /* Extract service_id from path query string */
        svc_id = 0;
        const char* sid_p = strstr(ws_path, "service_id=");
        if (sid_p) {
            svc_id = (int32_t)atol(sid_p + 11);
        }

        /* WebSocket connect */
        ws_free(&s_ws);
        if (ws_connect(&s_ws, ws_host, ws_path, s_access_token) != 0) {
            syslog(LOG_WARNING, "[%s] ws_connect failed; retry in 10s\n", TAG);
            sleep(10);
            continue;
        }

        /* Event receive loop */
        syslog(LOG_INFO, "[%s] Feishu Event Streaming active (svc_id=%ld)\n",
            TAG, (long)svc_id);

        while (s_ws.connected) {
            uint8_t opcode = 0;
            int n = ws_recv_frame(&s_ws, &opcode, (char*)frame_buf, frame_buf_size);

            if (n < 0) {
                if (feishu_token_expired()) {
                    break;
                }
                /* Socket timeout → send protobuf ping */
                if (svc_id) {
                    static uint8_t ping_buf[128];
                    int plen = fk_encode_ping(ping_buf, sizeof(ping_buf), svc_id);
                    if (plen > 0) {
                        ws_send_frame(&s_ws, 0x02, (const char*)ping_buf, (size_t)plen);
                    }
                } else {
                    ws_send_frame(&s_ws, 0x09, NULL, 0);
                }
                continue;
            }

            switch (opcode) {
            case 0x02: /* binary frame — protobuf pbbp2 */
                feishu_process_binary_frame(frame_buf, n, svc_id);
                break;
            case 0x01: /* text frame — unexpected */
                frame_buf[n < (int)frame_buf_size - 1 ? n : (int)frame_buf_size - 1] = 0;
                syslog(LOG_WARNING, "[%s] Text WS frame (unexpected): %.80s\n",
                    TAG, (char*)frame_buf);
                break;
            case 0x09: /* WS ping → pong */
                ws_send_frame(&s_ws, 0x0A, (const char*)frame_buf, (size_t)n);
                break;
            case 0x08: /* WS close */
                syslog(LOG_INFO, "[%s] Server WS close; reconnecting\n", TAG);
                s_ws.connected = false;
                break;
            default:
                break;
            }
        }

        ws_free(&s_ws);
        syslog(LOG_WARNING, "[%s] WS disconnected; reconnecting in 5s\n", TAG);
        sleep(5);
    }

    return NULL;
}

/* ── Public API ──────────────────────────────────────────────── */

int feishu_bot_init(void)
{
    memset(&s_ws, 0, sizeof(s_ws));
    mbedtls_net_init(&s_ws.net);
    sem_init(&s_cred_sem, 0, 0);

    /* Initialize RNG */
    if (!s_rng_ready) {
        mbedtls_ctr_drbg_init(&s_ctr_drbg);
        int ret = mbedtls_ctr_drbg_seed(&s_ctr_drbg, simple_entropy_func, NULL,
            (const unsigned char*)TAG, strlen(TAG));
        if (ret != 0) {
            syslog(LOG_ERR, "[%s] ctr_drbg_seed failed: -0x%04x\n", TAG, -ret);
            return ERROR;
        }
        s_rng_ready = true;
        syslog(LOG_INFO, "[%s] RNG ready\n", TAG);
    }

    char tmp[64] = { 0 };

    if (claw_config_get(AGENT_CFG_KEY_FEISHU_APP_ID, tmp, sizeof(tmp)) == OK &&
        tmp[0]) {
        strncpy(s_app_id, tmp, sizeof(s_app_id) - 1);
    }

    memset(tmp, 0, sizeof(tmp));
    if (claw_config_get(AGENT_CFG_KEY_FEISHU_APP_SECRET, tmp, sizeof(tmp)) == OK &&
        tmp[0]) {
        strncpy(s_app_secret, tmp, sizeof(s_app_secret) - 1);
    }

    memset(tmp, 0, sizeof(tmp));
    if (claw_config_get(AGENT_CFG_KEY_FEISHU_USER_TOKEN, tmp, sizeof(tmp)) == OK &&
        tmp[0]) {
        strncpy(s_user_token, tmp, sizeof(s_user_token) - 1);
        syslog(LOG_INFO, "[%s] Feishu user_token loaded\n", TAG);
    }

    if (s_app_id[0]) {
        syslog(LOG_INFO, "[%s] Feishu app_id=%s\n", TAG, s_app_id);
    } else {
        syslog(LOG_WARNING,
            "[%s] No Feishu credentials. Use CLI: set_feishu_app <app_id> <app_secret>\n",
            TAG);
    }

    return OK;
}

int feishu_bot_start(void)
{
    return agent_task_create(feishu_poll_task, "feishu_poll",
        AGENT_FEISHU_POLL_STACK, NULL, AGENT_FEISHU_POLL_PRIO);
}

int feishu_set_app(const char* app_id, const char* app_secret)
{
    strncpy(s_app_id, app_id, sizeof(s_app_id) - 1);
    strncpy(s_app_secret, app_secret, sizeof(s_app_secret) - 1);
    claw_config_set(AGENT_CFG_KEY_FEISHU_APP_ID, app_id);
    claw_config_set(AGENT_CFG_KEY_FEISHU_APP_SECRET, app_secret);

    s_access_token[0] = '\0';
    s_token_expire_ts = 0;

    sem_post(&s_cred_sem);

    syslog(LOG_INFO, "[%s] Feishu app credentials updated (app_id=%s)\n", TAG, app_id);
    return OK;
}

const char* feishu_get_app_id(void)
{
    return s_app_id;
}

int feishu_set_user_token(const char* token)
{
    if (!token || !token[0]) {
        return ERROR;
    }
    strncpy(s_user_token, token, sizeof(s_user_token) - 1);
    s_user_token[sizeof(s_user_token) - 1] = '\0';
    claw_config_set(AGENT_CFG_KEY_FEISHU_USER_TOKEN, token);
    syslog(LOG_INFO, "[%s] User access token set\n", TAG);
    return OK;
}


/* ── Public API for authenticated Feishu requests ────────────── */

int feishu_api_post(const char* path, const char* json_body, char* resp_buf,
    size_t resp_cap)
{
    if (!path || !resp_buf || resp_cap == 0) {
        return -1;
    }

    if (s_access_token[0] == '\0' || feishu_token_expired()) {
        if (feishu_get_app_token() != OK) {
            syslog(LOG_ERR, "[%s] feishu_api_post: token refresh failed\n", TAG);
            return -1;
        }
    }

    FEISHU_AUTH_HDR(hdrs, s_access_token);

    syslog(LOG_DEBUG, "[%s] feishu_api_post: %s\n", TAG, path);

    int status = feishu_https_post(path, hdrs, json_body, resp_buf, resp_cap);

    syslog(LOG_DEBUG, "[%s] feishu_api_post: status=%d\n", TAG, status);

    if ((status == 400 || status == 401)
        && strstr(resp_buf, "access token")) {
        syslog(LOG_INFO, "[%s] feishu_api_post: token expired, refreshing\n", TAG);
        if (feishu_get_app_token() == OK) {
            FEISHU_AUTH_HDR(retry_hdrs, s_access_token);
            status = feishu_https_post(path, retry_hdrs,
                json_body, resp_buf, resp_cap);
        }
    }

    return status;
}

int feishu_api_request(const char* method, const char* path, const char* body,
    size_t body_len, char* resp_buf, size_t resp_cap)
{
    if (!method || !path || !resp_buf || resp_cap == 0) {
        return -1;
    }

    if (s_access_token[0] == '\0' || feishu_token_expired()) {
        if (feishu_get_app_token() != OK) {
            syslog(LOG_ERR, "[%s] feishu_api_request: token refresh failed\n", TAG);
            return -1;
        }
    }

    char auth_val[520];
    snprintf(auth_val, sizeof(auth_val), "Bearer %s", s_access_token);
    vela_header_t hdrs[] = {
        { "Authorization", auth_val },
        { "Content-Type", "application/json" },
        { NULL, NULL }
    };

    size_t out_body_len = 0;
    int status = feishu_https_request(method, path, hdrs, body, body_len,
        resp_buf, resp_cap, &out_body_len);

    if ((status == 400 || status == 401)
        && strstr(resp_buf, "access token")) {
        syslog(LOG_INFO, "[%s] feishu_api_request: token expired, "
            "refreshing\n", TAG);
        if (feishu_get_app_token() == OK) {
            snprintf(auth_val, sizeof(auth_val), "Bearer %s", s_access_token);
            status = feishu_https_request(method, path, hdrs, body, body_len,
                resp_buf, resp_cap, &out_body_len);
        }
    }

    return status;
}

int feishu_api_post_as_user(const char* path, const char* json_body,
    char* resp_buf, size_t resp_cap)
{
    if (!path || !resp_buf || resp_cap == 0) {
        return -1;
    }

    if (s_user_token[0] == '\0') {
        char tmp[512] = { 0 };
        if (claw_config_get(AGENT_CFG_KEY_FEISHU_USER_TOKEN, tmp, sizeof(tmp)) == OK &&
            tmp[0]) {
            strncpy(s_user_token, tmp, sizeof(s_user_token) - 1);
        }
    }

    if (s_user_token[0] == '\0') {
        syslog(LOG_ERR,
            "[%s] No user_access_token set. Use CLI: set_feishu_user_token <token>\n",
            TAG);
        snprintf(resp_buf, resp_cap,
            "{\"code\":-1,\"msg\":\"No user_access_token. Use CLI: "
            "set_feishu_user_token <token>\"}");
        return -1;
    }

    FEISHU_AUTH_HDR(hdrs, s_user_token);

    syslog(LOG_DEBUG, "[%s] feishu_api_post_as_user: %s\n", TAG, path);

    int status = feishu_https_post(path, hdrs, json_body, resp_buf, resp_cap);

    syslog(LOG_DEBUG, "[%s] feishu_api_post_as_user: status=%d\n", TAG, status);

    return status;
}

int feishu_api_request_as_user(const char* method, const char* path,
    const char* body, size_t body_len,
    char* resp_buf, size_t resp_cap)
{
    if (!method || !path || !resp_buf || resp_cap == 0) {
        return -1;
    }

    if (s_user_token[0] == '\0') {
        char tmp[512] = { 0 };
        if (claw_config_get(AGENT_CFG_KEY_FEISHU_USER_TOKEN, tmp, sizeof(tmp)) == OK &&
            tmp[0]) {
            strncpy(s_user_token, tmp, sizeof(s_user_token) - 1);
        }
    }

    if (s_user_token[0] == '\0') {
        syslog(LOG_ERR, "[%s] No user_access_token set for request_as_user\n", TAG);
        snprintf(resp_buf, resp_cap,
            "{\"code\":-1,\"msg\":\"No user_access_token. Use CLI: "
            "set_feishu_user_token <token>\"}");
        return -1;
    }

    char auth_val[520];
    snprintf(auth_val, sizeof(auth_val), "Bearer %s", s_user_token);
    vela_header_t hdrs[3] = {
        { "Authorization", auth_val },
        { "Content-Type", "application/json" },
        { NULL, NULL }
    };

    syslog(LOG_DEBUG, "[%s] feishu_api_request_as_user: %s %s\n",
        TAG, method, path);

    size_t out_len = 0;
    int status = feishu_https_request(method, path, hdrs, body, body_len,
        resp_buf, resp_cap, &out_len);

    syslog(LOG_DEBUG, "[%s] feishu_api_request_as_user: status=%d\n",
        TAG, status);

    return status;
}
