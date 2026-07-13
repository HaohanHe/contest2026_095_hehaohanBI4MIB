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

#ifndef FEISHU_INTERNAL_H
#define FEISHU_INTERNAL_H

#include "channels/feishu_bot.h"
#include "infra/vela_tls.h"
#include "agent_compat.h"
#include "agent_config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* mbedTLS */
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── WebSocket client context ───────────────────────────────── */

typedef struct {
    mbedtls_net_context net;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    bool connected;
} feishu_ws_t;

/* ── Protobuf frame structure ───────────────────────────────── */

typedef struct {
    uint64_t seq_id;
    int32_t service;
    int32_t method; /* 0=control, 1=data */
    char h_type[32]; /* "event", "ping", "pong" */
    char h_msg_id[64]; /* message_id */
    char h_trace_id[64]; /* trace_id */
    int h_sum; /* multi-part total */
    int h_seq; /* multi-part seq */
    const uint8_t* payload;
    int payload_len;
} fk_frame_t;

/* ── Shared state (defined in feishu_bot.c) ─────────────────── */

extern char s_app_id[64];
extern char s_app_secret[64];
extern char s_access_token[512];
extern char s_user_token[512];
extern char s_bot_open_id[64];
extern int64_t s_token_expire_ts;
extern feishu_ws_t s_ws;
extern mbedtls_ctr_drbg_context s_ctr_drbg;
extern bool s_rng_ready;

/* ── feishu_ws.c ────────────────────────────────────────────── */

void ws_free(feishu_ws_t* ws);
int ws_connect(feishu_ws_t* ws, const char* host, const char* path,
    const char* token);
int ws_send_frame(feishu_ws_t* ws, uint8_t opcode, const void* data,
    size_t len);
int ws_recv_frame(feishu_ws_t* ws, uint8_t* opcode_out, char* buf,
    size_t cap);

/* ── feishu_http.c ──────────────────────────────────────────── */

int feishu_https_post(const char* path, const vela_header_t* extra_headers,
    const char* json_body, char* resp_buf, size_t resp_cap);
int feishu_https_request(const char* method, const char* path,
    const vela_header_t* headers, const char* body,
    size_t body_len, char* resp_buf, size_t resp_cap,
    size_t* out_body_len);
int feishu_get_app_token(void);
bool feishu_token_expired(void);
int feishu_create_connection(char* ws_host, size_t host_cap,
    char* ws_path, size_t path_cap);

/* ── feishu_proto.c ─────────────────────────────────────────── */

int pb_decode_frame(const uint8_t* buf, int len, fk_frame_t* f);
int pb_put_varint_field(uint8_t* buf, int pos, int cap, int fnum, uint64_t v);
int pb_put_len_field(uint8_t* buf, int pos, int cap, int fnum,
    const void* data, int dlen);
int pb_encode_header_entry(uint8_t* buf, int pos, int cap,
    const char* key, const char* val);
int fk_encode_ack(uint8_t* out, int cap, const fk_frame_t* req);
int fk_encode_ping(uint8_t* out, int cap, int32_t svc_id);

/* ── feishu_recv.c ──────────────────────────────────────────── */

void feishu_process_binary_frame(const uint8_t* data, int len, int32_t svc_id);

/* ── feishu_send.c ──────────────────────────────────────────── */

/* (feishu_send_message is declared in feishu_bot.h) */

/* ── Utility macros ─────────────────────────────────────────── */

#define FEISHU_AUTH_HDR(hdrs_var, token_var)                                \
    char hdrs_var##_auth_val[520];                                          \
    snprintf(hdrs_var##_auth_val, sizeof(hdrs_var##_auth_val), "Bearer %s", \
        (token_var));                                                       \
    vela_header_t hdrs_var[2] = { { "Authorization", hdrs_var##_auth_val }, \
        { NULL, NULL } }

#ifdef __cplusplus
}
#endif

#endif /* FEISHU_INTERNAL_H */
