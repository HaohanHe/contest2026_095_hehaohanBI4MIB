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
#include "infra/http_proxy.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "mbedtls/base64.h"

static const char* TAG = "feishu_ws";

/* ── Low-level TLS helpers ──────────────────────────────────── */

void ws_free(feishu_ws_t* ws)
{
    if (ws->connected) {
        mbedtls_ssl_close_notify(&ws->ssl);
    }
    mbedtls_ssl_free(&ws->ssl);
    mbedtls_ssl_config_free(&ws->conf);

    if (ws->net.fd >= 0) {
        close(ws->net.fd);
        ws->net.fd = -1;
    }

    mbedtls_net_init(&ws->net);
    mbedtls_ssl_init(&ws->ssl);
    mbedtls_ssl_config_init(&ws->conf);
    ws->connected = false;
}

/* Read exactly `len` bytes from TLS stream; returns -1 on error. */
static int ws_read_exact(feishu_ws_t* ws, uint8_t* buf, size_t len)
{
    size_t total = 0;

    while (total < len) {
        int n = mbedtls_ssl_read(&ws->ssl, buf + total, len - total);
        if (n == MBEDTLS_ERR_SSL_WANT_READ) {
            continue;
        }
        if (n <= 0) {
            return -1;
        }
        total += (size_t)n;
    }
    return (int)total;
}

/* Write `len` bytes to TLS stream; returns -1 on error. */
static int ws_write_all(feishu_ws_t* ws, const uint8_t* buf, size_t len)
{
    size_t written = 0;

    while (written < len) {
        int n = mbedtls_ssl_write(&ws->ssl, buf + written, len - written);
        if (n == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        }
        if (n <= 0) {
            return -1;
        }
        written += (size_t)n;
    }
    return (int)written;
}

/* ── WebSocket handshake ─────────────────────────────────────── */

int ws_connect(feishu_ws_t* ws, const char* host, const char* path,
    const char* token)
{
    mbedtls_net_init(&ws->net);
    mbedtls_ssl_init(&ws->ssl);
    mbedtls_ssl_config_init(&ws->conf);

    if (!s_rng_ready) {
        syslog(LOG_ERR, "[%s] ws_connect: RNG not initialised\n", TAG);
        goto fail;
    }

    int ret;

    /* TCP connect — via proxy tunnel if configured, direct otherwise */
    if (http_proxy_is_enabled()) {
        int sock = proxy_open_tunnel(host, 443, 30000);
        if (sock < 0) {
            syslog(LOG_ERR, "[%s] proxy_open_tunnel %s:443 failed\n", TAG, host);
            goto fail;
        }
        ws->net.fd = sock;
    } else {
        ret = mbedtls_net_connect(&ws->net, host, "443", MBEDTLS_NET_PROTO_TCP);
        if (ret != 0) {
            syslog(LOG_ERR, "[%s] net_connect %s: -0x%04x\n", TAG, host, -ret);
            goto fail;
        }
    }

    mbedtls_net_set_block(&ws->net);
    {
        struct timeval tv = { .tv_sec = 90, .tv_usec = 0 };
        setsockopt(ws->net.fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    /* TLS configuration */
    ret = mbedtls_ssl_config_defaults(&ws->conf, MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        syslog(LOG_ERR, "[%s] ssl_config_defaults: -0x%04x\n", TAG, -ret);
        goto fail;
    }

    mbedtls_ssl_conf_min_tls_version(&ws->conf, MBEDTLS_SSL_VERSION_TLS1_2);
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
    mbedtls_ssl_conf_max_tls_version(&ws->conf, MBEDTLS_SSL_VERSION_TLS1_3);
#else
    mbedtls_ssl_conf_max_tls_version(&ws->conf, MBEDTLS_SSL_VERSION_TLS1_2);
#endif

#if defined(MBEDTLS_SSL_ALPN)
    {
        static const char* alpn_protos[] = { "http/1.1", NULL };
        ret = mbedtls_ssl_conf_alpn_protocols(&ws->conf, alpn_protos);
        if (ret != 0) {
            syslog(LOG_WARNING, "[%s] alpn_protocols: -0x%04x (non-fatal)\n",
                TAG, -ret);
        }
    }
#endif

    mbedtls_ssl_conf_authmode(&ws->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_rng(&ws->conf, mbedtls_ctr_drbg_random, &s_ctr_drbg);

    ret = mbedtls_ssl_setup(&ws->ssl, &ws->conf);
    if (ret != 0) {
        syslog(LOG_ERR, "[%s] ssl_setup: -0x%04x\n", TAG, -ret);
        goto fail;
    }

    mbedtls_ssl_set_hostname(&ws->ssl, host);
    mbedtls_ssl_set_bio(&ws->ssl, &ws->net, mbedtls_net_send, mbedtls_net_recv,
        NULL);

    /* TLS handshake */
    while ((ret = mbedtls_ssl_handshake(&ws->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            syslog(LOG_ERR, "[%s] ssl_handshake: -0x%04x\n", TAG, -ret);
            goto fail;
        }
    }

    /* ── WebSocket upgrade ──────────────────────────────────── */
    uint8_t raw_key[16];

    ret = mbedtls_ctr_drbg_random(&s_ctr_drbg, raw_key, sizeof(raw_key));
    if (ret != 0) {
        syslog(LOG_ERR, "[%s] ctr_drbg_random for ws key: -0x%04x\n", TAG, -ret);
        goto fail;
    }

    unsigned char b64_key[32] = { 0 };
    size_t b64_len = 0;

    mbedtls_base64_encode(b64_key, sizeof(b64_key) - 1, &b64_len, raw_key, 16);
    b64_key[b64_len] = '\0';

    char* req = malloc(2048);

    if (!req) {
        syslog(LOG_ERR, "[%s] OOM for WS req\n", TAG);
        goto fail;
    }

    int rlen = snprintf(req, 2048,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Authorization: Bearer %s\r\n"
        "\r\n",
        path, host, (char*)b64_key, token);

    if (ws_write_all(ws, (uint8_t*)req, rlen) < 0) {
        syslog(LOG_ERR, "[%s] ws upgrade write error\n", TAG);
        free(req);
        goto fail;
    }
    free(req);

    /* Read upgrade response until \r\n\r\n */
    char* resp = malloc(2048);

    if (!resp) {
        syslog(LOG_ERR, "[%s] OOM for WS resp\n", TAG);
        goto fail;
    }
    memset(resp, 0, 2048);
    int resp_len = 0;
    uint8_t ch;

    while (resp_len < 2048 - 1) {
        if (ws_read_exact(ws, &ch, 1) < 0) {
            free(resp);
            goto fail;
        }
        resp[resp_len++] = (char)ch;
        if (resp_len >= 4 && memcmp(resp + resp_len - 4, "\r\n\r\n", 4) == 0) {
            break;
        }
    }
    resp[resp_len] = '\0';

    if (!strstr(resp, " 101 ")) {
        syslog(LOG_ERR, "[%s] WS upgrade failed (no 101): %.160s\n", TAG, resp);
        free(resp);
        goto fail;
    }
    free(resp);

    ws->connected = true;
    syslog(LOG_INFO, "[%s] WebSocket connected to wss://%s%s\n", TAG, host, path);
    return 0;

fail:
    ws_free(ws);
    return -1;
}

/* ── WebSocket frame I/O ─────────────────────────────────────── */

int ws_send_frame(feishu_ws_t* ws, uint8_t opcode, const void* data, size_t len)
{
    if (!ws->connected) {
        return -1;
    }

    uint8_t header[14];
    int hlen = 0;

    header[hlen++] = (uint8_t)(0x80 | (opcode & 0x0F)); /* FIN=1 */

    if (len < 126) {
        header[hlen++] = (uint8_t)(0x80 | len); /* MASK=1 */
    } else if (len < 65536) {
        header[hlen++] = (uint8_t)(0x80 | 126);
        header[hlen++] = (uint8_t)((len >> 8) & 0xFF);
        header[hlen++] = (uint8_t)(len & 0xFF);
    } else {
        header[hlen++] = (uint8_t)(0x80 | 127);
        for (int i = 7; i >= 0; i--) {
            header[hlen++] = (uint8_t)((len >> (i * 8)) & 0xFF);
        }
    }

    /* Masking key */
    uint8_t mask[4];

    mask[0] = (uint8_t)(rand() & 0xFF);
    mask[1] = (uint8_t)(rand() & 0xFF);
    mask[2] = (uint8_t)(rand() & 0xFF);
    mask[3] = (uint8_t)(rand() & 0xFF);
    header[hlen++] = mask[0];
    header[hlen++] = mask[1];
    header[hlen++] = mask[2];
    header[hlen++] = mask[3];

    if (ws_write_all(ws, header, hlen) < 0) {
        return -1;
    }

    /* Send masked payload in 256-byte chunks */
    const uint8_t* src = (const uint8_t*)data;
    uint8_t chunk[256];

    for (size_t offset = 0; offset < len;) {
        size_t todo = len - offset;
        if (todo > sizeof(chunk)) {
            todo = sizeof(chunk);
        }
        for (size_t i = 0; i < todo; i++) {
            chunk[i] = src[offset + i] ^ mask[(offset + i) & 3];
        }
        if (ws_write_all(ws, chunk, todo) < 0) {
            return -1;
        }
        offset += todo;
    }
    return 0;
}

int ws_recv_frame(feishu_ws_t* ws, uint8_t* opcode_out, char* buf, size_t cap)
{
    if (!ws->connected) {
        return -1;
    }

    uint8_t b0;
    uint8_t b1;

    if (ws_read_exact(ws, &b0, 1) < 0) {
        return -1;
    }
    if (ws_read_exact(ws, &b1, 1) < 0) {
        return -1;
    }

    *opcode_out = (uint8_t)(b0 & 0x0F);
    int masked = (b1 & 0x80) != 0;
    uint64_t payload_len = (uint64_t)(b1 & 0x7F);

    if (payload_len == 126) {
        uint8_t ext[2];
        if (ws_read_exact(ws, ext, 2) < 0) {
            return -1;
        }
        payload_len = ((uint64_t)ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        if (ws_read_exact(ws, ext, 8) < 0) {
            return -1;
        }
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | ext[i];
        }
    }

    uint8_t mask[4] = { 0 };

    if (masked) {
        if (ws_read_exact(ws, mask, 4) < 0) {
            return -1;
        }
    }

    /* Clamp to buffer, discard overflow */
    size_t read_len = (payload_len < cap - 1) ? (size_t)payload_len : (cap - 1);
    size_t total = 0;

    while (total < read_len) {
        int n = mbedtls_ssl_read(&ws->ssl, (uint8_t*)buf + total,
            (int)(read_len - total));
        if (n == MBEDTLS_ERR_SSL_WANT_READ) {
            continue;
        }
        if (n <= 0) {
            return -1;
        }
        total += (size_t)n;
    }

    /* Discard any overflow bytes */
    uint8_t discard;

    for (uint64_t i = read_len; i < payload_len; i++) {
        if (ws_read_exact(ws, &discard, 1) < 0) {
            return -1;
        }
    }

    if (masked) {
        for (size_t i = 0; i < total; i++) {
            buf[i] ^= mask[i & 3];
        }
    }

    buf[total] = '\0';
    return (int)total;
}
