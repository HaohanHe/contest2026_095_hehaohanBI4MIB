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

/*
 * This file contains code derived from MimiClaw (https://github.com/memovai/mimiclaw)
 * Copyright (c) 2026 Ziboyan Wang, licensed under the MIT License.
 * See NOTICE file for the original MIT License terms.
 */

#include "infra/http_proxy.h"
#include "infra/config_store.h"
#include "agent_config.h"
#include "agent_compat.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"

#include <fcntl.h>
#include <time.h>
#include <sys/select.h>

static const char *TAG = "proxy";

/* Entropy source for TLS — uses agent_secure_random() from compat layer */
static int proxy_entropy_func(void *data, unsigned char *output, size_t len)
{
    (void)data;
    if (agent_secure_random(output, len) == 0) {
        return 0;
    }
    /* No fallback — cryptographic entropy is mandatory for TLS.
     * Returning an error forces the TLS handshake to fail safely
     * rather than proceeding with predictable key material. */
    syslog(LOG_ERR, "[%s] CRITICAL: No secure entropy source available\n", TAG);
    return -1;  /* Generic error - TLS handshake will fail safely */
}

static char     s_proxy_host[64] = {0};
static uint16_t s_proxy_port     = 0;

/* ── Init ────────────────────────────────────────────────────── */

int http_proxy_init(void)
{
    /* Build-time defaults */
    if (AGENT_SECRET_PROXY_HOST[0] != '\0' && AGENT_SECRET_PROXY_PORT[0] != '\0') {
        strncpy(s_proxy_host, AGENT_SECRET_PROXY_HOST, sizeof(s_proxy_host) - 1);
        s_proxy_port = (uint16_t)atoi(AGENT_SECRET_PROXY_PORT);
    }

    syslog(LOG_INFO, "[%s] Loading proxy config...\n", TAG);
    char tmp[64] = {0};
    if (claw_config_get(AGENT_CFG_KEY_PROXY_HOST, tmp, sizeof(tmp)) == OK && tmp[0]) {
        strncpy(s_proxy_host, tmp, sizeof(s_proxy_host) - 1);
        syslog(LOG_INFO, "[%s] Proxy host loaded: %s\n", TAG, s_proxy_host);
    }

    memset(tmp, 0, sizeof(tmp));
    if (claw_config_get(AGENT_CFG_KEY_PROXY_PORT, tmp, sizeof(tmp)) == OK && tmp[0]) {
        s_proxy_port = (uint16_t)atoi(tmp);
        syslog(LOG_INFO, "[%s] Proxy port loaded: %d\n", TAG, s_proxy_port);
    }

    if (s_proxy_host[0] && s_proxy_port) {
        syslog(LOG_INFO, "[%s] Proxy configured: %s:%d\n", TAG, s_proxy_host, s_proxy_port);
    } else {
        syslog(LOG_INFO, "[%s] No proxy (direct connection)\n", TAG);
    }
    return OK;
}

bool http_proxy_is_enabled(void)
{
    return s_proxy_host[0] != '\0' && s_proxy_port != 0;
}

int http_proxy_set(const char *host, uint16_t port)
{
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);
    claw_config_set(AGENT_CFG_KEY_PROXY_HOST, host);
    claw_config_set(AGENT_CFG_KEY_PROXY_PORT, port_str);

    strncpy(s_proxy_host, host, sizeof(s_proxy_host) - 1);
    s_proxy_port = port;
    syslog(LOG_INFO, "[%s] Proxy set to %s:%d\n", TAG, s_proxy_host, s_proxy_port);
    return OK;
}

int http_proxy_clear(void)
{
    config_del(AGENT_CFG_KEY_PROXY_HOST);
    config_del(AGENT_CFG_KEY_PROXY_PORT);
    s_proxy_host[0] = '\0';
    s_proxy_port = 0;
    syslog(LOG_INFO, "[%s] Proxy cleared\n", TAG);
    return OK;
}

/* ── proxy_conn_t ─────────────────────────────────────────────── */

struct proxy_conn {
    int  sock;   /* raw TCP fd (kept so we can set SO_RCVTIMEO) */
    mbedtls_ssl_context      ssl;
    mbedtls_ssl_config       cfg;
    mbedtls_net_context      net;
    mbedtls_ctr_drbg_context ctr_drbg;
};

/* Read one line (CR-LF stripped) from raw socket. Returns len or -1. */
static int sock_read_line(int fd, char *buf, int maxlen, int timeout_ms)
{
    struct timeval tv = { .tv_sec = timeout_ms / 1000,
                          .tv_usec = (timeout_ms % 1000) * 1000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int pos = 0;
    while (pos < maxlen - 1) {
        char c;
        int r = recv(fd, &c, 1, 0);
        if (r <= 0) return -1;
        if (c == '\n') { buf[pos] = '\0'; return pos; }
        if (c != '\r') buf[pos++] = c;
    }
    buf[pos] = '\0';
    return pos;
}

/* Open TCP connection to proxy + send CONNECT, returns fd or -1. */
static int open_connect_tunnel(const char *host, int port, int timeout_ms)
{
    struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM };
    struct addrinfo *res = NULL;
    char proxy_port_str[8];
    snprintf(proxy_port_str, sizeof(proxy_port_str), "%d", s_proxy_port);

    if (getaddrinfo(s_proxy_host, proxy_port_str, &hints, &res) != 0 || !res) {
        syslog(LOG_ERR, "[%s] DNS failed for proxy %s\n", TAG, s_proxy_host);
        return -1;
    }

    int sock = socket(res->ai_family, SOCK_STREAM, 0);
    if (sock < 0) { freeaddrinfo(res); return -1; }

    /* Non-blocking connect with select() timeout — SO_SNDTIMEO does not
     * affect connect() on all platforms (NuttX sim included). */
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    int rc = connect(sock, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (rc != 0 && errno != EINPROGRESS) {
        syslog(LOG_ERR, "[%s] TCP connect to proxy %s:%d failed: %s\n",
               TAG, s_proxy_host, s_proxy_port, strerror(errno));
        close(sock);
        return -1;
    }

    /* Wait for connect to complete */
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sock, &wfds);
    struct timeval tv = { .tv_sec = timeout_ms / 1000,
                          .tv_usec = (timeout_ms % 1000) * 1000 };
    rc = select(sock + 1, NULL, &wfds, NULL, &tv);
    if (rc <= 0) {
        syslog(LOG_ERR, "[%s] TCP connect timeout to proxy %s:%d\n",
               TAG, s_proxy_host, s_proxy_port);
        close(sock);
        return -1;
    }

    /* Check SO_ERROR to confirm connect succeeded */
    int err = 0;
    socklen_t errlen = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &errlen);
    if (err != 0) {
        syslog(LOG_ERR, "[%s] TCP connect to proxy %s:%d failed: %s\n",
               TAG, s_proxy_host, s_proxy_port, strerror(err));
        close(sock);
        return -1;
    }

    /* Restore blocking mode for subsequent send/recv */
    fcntl(sock, F_SETFL, flags);

    /* Send CONNECT request */
    char req[256];
    int len = snprintf(req, sizeof(req),
        "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n\r\n",
        host, port, host, port);
    if (send(sock, req, len, 0) != len) {
        syslog(LOG_ERR, "[%s] CONNECT send failed\n", TAG); close(sock); return -1;
    }

    /* Parse response */
    char line[256];
    if (sock_read_line(sock, line, sizeof(line), timeout_ms) < 0) {
        syslog(LOG_ERR, "[%s] No proxy response\n", TAG); close(sock); return -1;
    }
    if (!strstr(line, "200")) {
        syslog(LOG_ERR, "[%s] CONNECT rejected: %s\n", TAG, line); close(sock); return -1;
    }
    /* Consume remaining headers */
    while (sock_read_line(sock, line, sizeof(line), timeout_ms) > 0) { }

    return sock;
}

/* ── Public: proxy_conn_open ─────────────────────────────────── */

proxy_conn_t *proxy_conn_open(const char *host, int port, int timeout_ms)
{
    if (!http_proxy_is_enabled()) {
        syslog(LOG_ERR, "[%s] proxy_conn_open: no proxy configured\n", TAG);
        return NULL;
    }

    int sock = open_connect_tunnel(host, port, timeout_ms);
    if (sock < 0) return NULL;

    proxy_conn_t *conn = calloc(1, sizeof(*conn));
    if (!conn) { close(sock); return NULL; }

    conn->sock = sock;

    /* mbedTLS init */
    mbedtls_ssl_init(&conn->ssl);
    mbedtls_ssl_config_init(&conn->cfg);
    mbedtls_net_init(&conn->net);
    mbedtls_ctr_drbg_init(&conn->ctr_drbg);

    /* Seed RNG using portable entropy source (works on NuttX/QEMU) */
    const char *pers = "proxy_tls";
    int ret = mbedtls_ctr_drbg_seed(&conn->ctr_drbg, proxy_entropy_func,
                                     NULL,
                                     (const unsigned char *)pers, strlen(pers));
    if (ret != 0) {
        syslog(LOG_ERR, "[%s] ctr_drbg_seed failed: 0x%x\n", TAG, -ret);
        goto fail;
    }

    /* Inject our already-connected socket into mbedtls_net_context */
    conn->net.fd = sock;

    /* SSL config */
    ret = mbedtls_ssl_config_defaults(&conn->cfg,
                                       MBEDTLS_SSL_IS_CLIENT,
                                       MBEDTLS_SSL_TRANSPORT_STREAM,
                                       MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) { syslog(LOG_ERR, "[%s] ssl_config_defaults: 0x%x\n", TAG, -ret); goto fail; }

    mbedtls_ssl_conf_authmode(&conn->cfg, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_rng(&conn->cfg, mbedtls_ctr_drbg_random, &conn->ctr_drbg);

    ret = mbedtls_ssl_setup(&conn->ssl, &conn->cfg);
    if (ret != 0) { syslog(LOG_ERR, "[%s] ssl_setup: 0x%x\n", TAG, -ret); goto fail; }

    ret = mbedtls_ssl_set_hostname(&conn->ssl, host);
    if (ret != 0) { syslog(LOG_ERR, "[%s] ssl_set_hostname: 0x%x\n", TAG, -ret); goto fail; }

    mbedtls_ssl_set_bio(&conn->ssl, &conn->net,
                        mbedtls_net_send, mbedtls_net_recv, NULL);

    /* Handshake */
    do {
        ret = mbedtls_ssl_handshake(&conn->ssl);
    } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (ret != 0) {
        syslog(LOG_ERR, "[%s] TLS handshake failed over proxy: 0x%x\n", TAG, -ret);
        goto fail;
    }

    syslog(LOG_INFO, "[%s] TLS handshake OK with %s:%d via proxy\n", TAG, host, port);
    return conn;

fail:
    mbedtls_ssl_free(&conn->ssl);
    mbedtls_ssl_config_free(&conn->cfg);
    /* Don't call mbedtls_net_free — that would close sock again */
    conn->net.fd = -1;
    mbedtls_ctr_drbg_free(&conn->ctr_drbg);
    close(sock);
    free(conn);
    return NULL;
}

int proxy_conn_write(proxy_conn_t *conn, const char *data, int len)
{
    int written = 0;
    while (written < len) {
        int ret = mbedtls_ssl_write(&conn->ssl,
                                    (const unsigned char *)(data + written),
                                    (size_t)(len - written));
        if (ret > 0)  { written += ret; }
        else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) { continue; }
        else { syslog(LOG_ERR, "[%s] ssl_write error: 0x%x\n", TAG, -ret); return -1; }
    }
    return written;
}

int proxy_conn_read(proxy_conn_t *conn, char *buf, int len, int timeout_ms)
{
    struct timeval tv = { .tv_sec = timeout_ms / 1000,
                          .tv_usec = (timeout_ms % 1000) * 1000 };
    setsockopt(conn->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int ret = mbedtls_ssl_read(&conn->ssl, (unsigned char *)buf, (size_t)len);
    if (ret == MBEDTLS_ERR_SSL_WANT_READ) return 0;
    if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return 0;
    if (ret < 0) { syslog(LOG_ERR, "[%s] ssl_read error: 0x%x\n", TAG, -ret); return -1; }
    return ret;
}

void proxy_conn_close(proxy_conn_t *conn)
{
    if (!conn) return;
    mbedtls_ssl_close_notify(&conn->ssl);
    mbedtls_ssl_free(&conn->ssl);
    mbedtls_ssl_config_free(&conn->cfg);
    /* Close the raw socket (mbedtls_net_free would also do this) */
    if (conn->net.fd >= 0) {
        close(conn->net.fd);
        conn->net.fd = -1;
    }
    mbedtls_ctr_drbg_free(&conn->ctr_drbg);
    free(conn);
}

int proxy_open_tunnel(const char *host, int port, int timeout_ms)
{
    if (!http_proxy_is_enabled()) {
        syslog(LOG_ERR, "[%s] proxy_open_tunnel: no proxy configured\n", TAG);
        return -1;
    }
    return open_connect_tunnel(host, port, timeout_ms);
}
