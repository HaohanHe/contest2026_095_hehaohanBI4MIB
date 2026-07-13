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

#pragma once

#include "agent_compat.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize proxy module (reads AGENT_SECRET_PROXY_* + config_store).
 */
int http_proxy_init(void);

/** Returns true if a proxy host:port is configured. */
bool http_proxy_is_enabled(void);

/** Save proxy host + port to config_store. */
int http_proxy_set(const char *host, uint16_t port);

/** Remove proxy config from config_store. */
int http_proxy_clear(void);

/* ── Proxied HTTPS connection ──────────────────────────────────── */

typedef struct proxy_conn proxy_conn_t;

/**
 * Open an HTTPS connection through the configured proxy:
 *  1) TCP to proxy
 *  2) HTTP CONNECT tunnel to target
 *  3) mbedTLS handshake over the tunnel
 *
 * Returns NULL on failure.
 */
proxy_conn_t *proxy_conn_open(const char *host, int port, int timeout_ms);

/** Write raw bytes through the TLS tunnel. Returns written or -1. */
int proxy_conn_write(proxy_conn_t *conn, const char *data, int len);

/** Read raw bytes from the TLS tunnel. Returns read or -1. */
int proxy_conn_read(proxy_conn_t *conn, char *buf, int len, int timeout_ms);

/** Close and free the connection. */
void proxy_conn_close(proxy_conn_t *conn);

/**
 * Open a raw TCP tunnel through the configured HTTP proxy (CONNECT).
 * Returns a connected socket fd, or -1 on failure.
 * The caller owns the fd and must close() it.
 * Useful when the caller needs to do its own TLS handshake (e.g. WebSocket).
 */
int proxy_open_tunnel(const char *host, int port, int timeout_ms);

#ifdef __cplusplus
}
#endif
