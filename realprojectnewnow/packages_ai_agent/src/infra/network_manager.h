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
/**
 * network_manager.h — Vela network status helper
 *
 * managed by the system (netinit / board BSP).  This module simply
 * waits for any non-loopback interface to get an IPv4 address.
 */

#include "agent_compat.h"

/** Check whether the system has a routable IPv4 address. */
bool network_is_connected(void);

/**
 * Block until connected (or timeout).
 * @param timeout_ms  0 = check once, UINT32_MAX = wait forever.
 * @return OK on success, ERROR otherwise.
 */
int network_wait_connected(uint32_t timeout_ms);

/** Return a static string with the first non-loopback IPv4 address. */
const char *network_get_ip(void);

/**
 * Connect to a WiFi network using the NuttX wapi tool.
 * On QEMU this is a no-op (network is provided by virtio-net).
 * Saves credentials to config_store for persistence across reboots.
 * @param iface  WiFi interface name e.g. "wlan0" (NULL = use "wlan0")
 */
int network_wifi_connect(const char *iface, const char *ssid, const char *pass);

/** Re-connect using credentials saved in config_store (called at startup). */
int network_wifi_reconnect(void);

/* ── RPMSG/TUN network interface ──────────────────────────────── */
#ifdef CONFIG_AI_AGENT_NET_RPMSG

#include <arpa/inet.h>

/** Network state enumeration */
typedef enum {
    NET_STATE_DISCONNECTED = 0,
    NET_STATE_CONNECTED = 1,
} net_state_t;

/** Network state change callback */
typedef void (*net_state_cb_t)(net_state_t state, void* arg);

/** Internal network status structure */
typedef struct {
    net_state_t state;
    char ip_addr[INET_ADDRSTRLEN];
    char iface_name[16];
    bool ble_connected;
    int active_conns;
    int iob_usage_pct;
    time_t last_check;
    time_t connected_since;
} net_status_t;

/* Resource constraint macros */
#define NET_MAX_TCP_CONNS 8
#define NET_TLS_BUF_LIMIT (64 * 1024)
#define NET_IOB_HIGH_WATERMARK 75
#define NET_IOB_LOW_WATERMARK 50
#define NET_MSG_QUEUE_TIMEOUT 300
#define NET_POLL_INTERVAL_MS 5000
#define NET_MAX_LISTENERS 8
#define NET_STARTUP_TIMEOUT_MS 30000
#define NET_BLE_REACT_MS 1000
#define NET_IP_DETECT_MS 500

/** Register a network state change listener (max 8). */
int network_register_listener(net_state_cb_t cb, void* arg);

/** Get current network state. */
net_state_t network_get_state(void);

/** Get active TCP connection count. */
int network_get_active_conns(void);

/** Get IOB usage percentage (0-100). */
int network_get_iob_usage(void);

/** Initialize RPMSG/TUN network detection (O74I only). */
int network_rpmsg_init(void);

/** Trigger network re-detection. */
int network_reconnect(void);

/** Configure DNS servers. */
int network_set_dns(const char* primary, const char* secondary);

/** Network diagnostics CLI handler. */
int network_diag(int argc, char** argv);

/** Save proxy config (mode + cpu_name) to config_store. */
int network_save_proxy_config(const char* mode, const char* cpu_name);

/** Get TLS connect timeout (seconds). */
int network_get_connect_timeout(void);

/** Get TLS read timeout (seconds). */
int network_get_read_timeout(void);

/** Get HTTP max retry count. */
int network_get_retry_max(void);

/** Get retry backoff base (seconds). */
int network_get_retry_base_sec(void);

/** Get current proxy mode ("usrsock" or "tun"). */
const char* network_get_proxy_mode(void);

/** Get RPMSG target CPU name. */
const char* network_get_rpmsg_cpu(void);

/** Acquire network resource permit (blocks until available or timeout). */
int network_acquire_resource(uint32_t timeout_ms);

/** Release network resource permit. */
void network_release_resource(void);

#endif /* CONFIG_AI_AGENT_NET_RPMSG */
