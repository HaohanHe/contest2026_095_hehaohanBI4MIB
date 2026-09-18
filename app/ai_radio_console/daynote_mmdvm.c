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
 * daynote_mmdvm.c - MMDVM Homebrew DMR 网络客户端实现
 *
 * 纯 UDP 协议，连接 BrandMeister master
 * 协议参考: g4klx/MMDVMHost DMRNetwork.cpp
 *
 * 功能:
 * - BrandMeister Homebrew 协议 (DMRD/DMRP)
 * - 自动重连 + 网络等待
 * - 通话状态跟踪
 * - 通联日志持久化 (dmr_log.json)
 * - 配置持久化 (通过 config_store)
 */

#include "daynote_mmdvm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <syslog.h>

#include <netutils/cJSON.h>

/* Forward declarations */
static void dmr_log_add(const dmr_call_t* call);

/* ===== 协议常量 (MMDVM Homebrew) ===== */

#define HOMEBREW_DATA_LEN   55U
#define HOMEBREW_PING_LEN   4U

#define TAG_DMRD            "DMRD"
#define TAG_DMRP            "DMRP"
#define TAG_DMRB            "DMRB"

#define CTRL_SLOT2          0x80U
#define CTRL_FLCO_USER      0x40U
#define CTRL_DATA_SYNC      0x20U
#define CTRL_VOICE_SYNC     0x10U
#define CTRL_TYPE_MASK      0x0FU

#define PING_INTERVAL_MS    10000U
#define RECV_TIMEOUT_SEC    1
#define RECONNECT_INTERVAL_MS 15000U
#define NET_WAIT_TIMEOUT_MS 30000U

#define DMR_LOG_FILENAME    "dmr_log.json"
#define DMR_LOG_MAX_ENTRIES 200
#define DMR_LOG_PATH_MAX    256

/* ===== 内部数据 ===== */

typedef struct {
    dmr_call_t  calls[DMR_LOG_MAX_ENTRIES];
    int         count;
    int         head;
    char        file_path[DMR_LOG_PATH_MAX];
    char        tmp_path[DMR_LOG_PATH_MAX];
    pthread_mutex_t lock;
    bool        initialized;
    bool        dirty;
} dmr_log_store_t;

static dmr_log_store_t g_dmr_log;

/* ===== 工具函数 ===== */

static uint64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static int64_t get_unix_time(void)
{
    return (int64_t)time(NULL);
}

/* ===== 通联日志持久化 ===== */

static int dmr_log_init(const char* data_dir)
{
    int n;

    memset(&g_dmr_log, 0, sizeof(g_dmr_log));
    pthread_mutex_init(&g_dmr_log.lock, NULL);

    n = snprintf(g_dmr_log.file_path, sizeof(g_dmr_log.file_path),
        "%s/%s", data_dir, DMR_LOG_FILENAME);
    if (n < 0 || (size_t)n >= sizeof(g_dmr_log.file_path)) {
        return -ENAMETOOLONG;
    }

    n = snprintf(g_dmr_log.tmp_path, sizeof(g_dmr_log.tmp_path),
        "%s/%s.tmp", data_dir, DMR_LOG_FILENAME);
    if (n < 0 || (size_t)n >= sizeof(g_dmr_log.tmp_path)) {
        return -ENAMETOOLONG;
    }

    g_dmr_log.initialized = true;
    dmr_log_load();
    return 0;
}

static void dmr_log_add(const dmr_call_t* call)
{
    if (!g_dmr_log.initialized || !call) return;

    pthread_mutex_lock(&g_dmr_log.lock);

    if (g_dmr_log.count < DMR_LOG_MAX_ENTRIES) {
        memcpy(&g_dmr_log.calls[g_dmr_log.count], call, sizeof(dmr_call_t));
        g_dmr_log.count++;
    } else {
        memcpy(&g_dmr_log.calls[g_dmr_log.head], call, sizeof(dmr_call_t));
        g_dmr_log.head = (g_dmr_log.head + 1) % DMR_LOG_MAX_ENTRIES;
    }

    g_dmr_log.dirty = true;
    pthread_mutex_unlock(&g_dmr_log.lock);
}

int dmr_log_save(void)
{
    cJSON* root;
    cJSON* arr;
    char* json_str;
    int fd;
    ssize_t written;
    size_t json_len;
    int i;

    if (!g_dmr_log.initialized || !g_dmr_log.dirty) return 0;

    root = cJSON_CreateObject();
    if (!root) return -ENOMEM;

    arr = cJSON_AddArrayToObject(root, "calls");
    if (!arr) {
        cJSON_Delete(root);
        return -ENOMEM;
    }

    pthread_mutex_lock(&g_dmr_log.lock);

    for (i = 0; i < g_dmr_log.count; i++) {
        int idx = (g_dmr_log.head + i) % DMR_LOG_MAX_ENTRIES;
        dmr_call_t* c = &g_dmr_log.calls[idx];
        cJSON* obj = cJSON_CreateObject();
        if (!obj) continue;

        cJSON_AddNumberToObject(obj, "src_id", (double)c->src_id);
        cJSON_AddNumberToObject(obj, "dst_id", (double)c->dst_id);
        cJSON_AddNumberToObject(obj, "slot", c->slot);
        cJSON_AddNumberToObject(obj, "call_type", c->call_type);
        cJSON_AddStringToObject(obj, "callsign", c->src_callsign);
        cJSON_AddStringToObject(obj, "name", c->src_name);
        cJSON_AddNumberToObject(obj, "duration_ms", (double)c->duration_ms);
        cJSON_AddNumberToObject(obj, "timestamp", (double)c->start_time);
        cJSON_AddNumberToObject(obj, "rssi", c->rssi);
        cJSON_AddNumberToObject(obj, "ber", (double)c->ber);
        cJSON_AddItemToArray(arr, obj);
    }

    pthread_mutex_unlock(&g_dmr_log.lock);

    json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) return -ENOMEM;

    json_len = strlen(json_str);

    fd = open(g_dmr_log.tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        free(json_str);
        return -errno;
    }

    written = write(fd, json_str, json_len);
    close(fd);
    free(json_str);

    if (written < 0 || (size_t)written != json_len) {
        unlink(g_dmr_log.tmp_path);
        return -EIO;
    }

    if (rename(g_dmr_log.tmp_path, g_dmr_log.file_path) != 0) {
        unlink(g_dmr_log.tmp_path);
        return -errno;
    }

    g_dmr_log.dirty = false;
    return 0;
}

int dmr_log_load(void)
{
    struct stat st;
    int fd;
    char* buf;
    ssize_t nread;
    cJSON* root;
    cJSON* arr;
    int arr_size;
    int i;

    if (!g_dmr_log.initialized) return -EINVAL;

    if (stat(g_dmr_log.file_path, &st) != 0) return 0;
    if (st.st_size == 0 || st.st_size > 512 * 1024) return -EINVAL;

    buf = malloc(st.st_size + 1);
    if (!buf) return -ENOMEM;

    fd = open(g_dmr_log.file_path, O_RDONLY);
    if (fd < 0) { free(buf); return -errno; }

    nread = read(fd, buf, st.st_size);
    close(fd);

    if (nread != st.st_size) { free(buf); return -EIO; }
    buf[st.st_size] = '\0';

    root = cJSON_Parse(buf);
    free(buf);
    if (!root) return -EINVAL;

    arr = cJSON_GetObjectItem(root, "calls");
    if (!arr || !cJSON_IsArray(arr)) {
        cJSON_Delete(root);
        return -EINVAL;
    }

    arr_size = cJSON_GetArraySize(arr);
    if (arr_size > DMR_LOG_MAX_ENTRIES) arr_size = DMR_LOG_MAX_ENTRIES;

    pthread_mutex_lock(&g_dmr_log.lock);
    g_dmr_log.count = 0;

    for (i = 0; i < arr_size; i++) {
        cJSON* obj = cJSON_GetArrayItem(arr, i);
        cJSON* f;
        dmr_call_t* c = &g_dmr_log.calls[g_dmr_log.count];
        memset(c, 0, sizeof(dmr_call_t));

        f = cJSON_GetObjectItem(obj, "src_id");
        if (f) c->src_id = (uint32_t)f->valuedouble;
        f = cJSON_GetObjectItem(obj, "dst_id");
        if (f) c->dst_id = (uint32_t)f->valuedouble;
        f = cJSON_GetObjectItem(obj, "slot");
        if (f) c->slot = (uint8_t)f->valueint;
        f = cJSON_GetObjectItem(obj, "call_type");
        if (f) c->call_type = (uint8_t)f->valueint;
        f = cJSON_GetObjectItem(obj, "callsign");
        if (f && f->valuestring)
            strncpy(c->src_callsign, f->valuestring, sizeof(c->src_callsign) - 1);
        f = cJSON_GetObjectItem(obj, "name");
        if (f && f->valuestring)
            strncpy(c->src_name, f->valuestring, sizeof(c->src_name) - 1);
        f = cJSON_GetObjectItem(obj, "duration_ms");
        if (f) c->duration_ms = (uint32_t)f->valuedouble;
        f = cJSON_GetObjectItem(obj, "timestamp");
        if (f) c->start_time = (uint64_t)f->valuedouble;
        f = cJSON_GetObjectItem(obj, "rssi");
        if (f) c->rssi = (uint8_t)f->valueint;
        f = cJSON_GetObjectItem(obj, "ber");
        if (f) c->ber = (uint8_t)(float)f->valuedouble;

        g_dmr_log.count++;
    }

    pthread_mutex_unlock(&g_dmr_log.lock);
    cJSON_Delete(root);
    return 0;
}

int mmdvm_get_log(dmr_call_t* buf, int max_count)
{
    int count;
    int i;

    if (!g_dmr_log.initialized || !buf || max_count <= 0) return 0;

    pthread_mutex_lock(&g_dmr_log.lock);
    count = g_dmr_log.count < max_count ? g_dmr_log.count : max_count;

    for (i = 0; i < count; i++) {
        int idx = (g_dmr_log.head + g_dmr_log.count - 1 - i) % DMR_LOG_MAX_ENTRIES;
        memcpy(&buf[i], &g_dmr_log.calls[idx], sizeof(dmr_call_t));
    }

    pthread_mutex_unlock(&g_dmr_log.lock);
    return count;
}

/* ===== DMR 帧解析 ===== */

static void parse_dmr_data(mmdvm_client_t* client, const uint8_t* buf, int len)
{
    uint32_t src_id, dst_id;
    uint8_t ctrl, slot, frame_type;
    bool group, data_sync, voice_sync;
    dmr_call_t* call;

    if (len < 20) return;

    src_id  = ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) | buf[7];
    dst_id  = ((uint32_t)buf[8] << 16) | ((uint32_t)buf[9] << 8) | buf[10];
    ctrl    = buf[15];
    slot    = (ctrl & CTRL_SLOT2) ? 1 : 0;
    group   = !(ctrl & CTRL_FLCO_USER);
    data_sync  = (ctrl & CTRL_DATA_SYNC) != 0;
    voice_sync = (ctrl & CTRL_VOICE_SYNC) != 0;
    frame_type = ctrl & CTRL_TYPE_MASK;

    (void)voice_sync;

    pthread_mutex_lock(&client->mutex);
    call = &client->current_call[slot];

    /* 新通话检测 */
    if (!call->active || call->src_id != src_id || call->dst_id != dst_id) {
        if (call->active && call->frame_count > 2) {
            call->duration_ms = (uint32_t)(get_time_ms() -
                client->call_start_ms[slot]);
            dmr_log_add(call);
        }

        memset(call, 0, sizeof(dmr_call_t));
        call->src_id      = src_id;
        call->dst_id      = dst_id;
        call->slot        = slot;
        call->call_type   = group ? DMR_CALL_GROUP : DMR_CALL_PRIVATE;
        call->start_time  = get_unix_time();
        call->active      = true;
        call->frame_count = 0;
        client->call_start_ms[slot] = get_time_ms();
        strncpy(call->src_callsign, dmr_id_to_callsign(src_id),
            MMDVM_CALLSIGN_LEN - 1);
    }

    call->frame_count++;
    if (len >= 54) call->ber  = buf[53];
    if (len >= 55) call->rssi = buf[54];
    call->duration_ms = (uint32_t)(get_time_ms() -
        client->call_start_ms[slot]);

    /* 终止帧结束通话 */
    if (data_sync && frame_type == DMR_FRAME_TERMINATOR) {
        call->active = false;
        call->duration_ms = (uint32_t)(get_time_ms() -
            client->call_start_ms[slot]);
        if (call->frame_count > 2) {
            dmr_log_add(call);
        }
        memset(call, 0, sizeof(dmr_call_t));
    }

    client->last_rx_ms = get_time_ms();
    client->rx_frames++;
    pthread_mutex_unlock(&client->mutex);
}

/* ===== 网络发送 ===== */

static int mmdvm_send_raw(mmdvm_client_t* client, const uint8_t* data, int len)
{
    int sent;

    if (client->sock_fd < 0 || !client->addr_valid) return -1;

    sent = sendto(client->sock_fd, data, len, 0,
        (struct sockaddr*)&client->master_sin, sizeof(client->master_sin));
    return (sent > 0) ? 0 : -1;
}

static int mmdvm_send_ping(mmdvm_client_t* client)
{
    static const uint8_t ping[] = {'D','M','R','P'};
    return mmdvm_send_raw(client, ping, sizeof(ping));
}

/* ===== 接收线程 ===== */

static void* mmdvm_recv_thread(void* arg)
{
    mmdvm_client_t* client = (mmdvm_client_t*)arg;
    uint8_t buf[MMDVM_BUF_SIZE];
    struct sockaddr_in from_addr;
    socklen_t from_len;

    while (client->thread_running) {
        struct timeval tv;
        int len;

        if (client->sock_fd < 0) {
            usleep(500000);
            continue;
        }

        tv.tv_sec  = RECV_TIMEOUT_SEC;
        tv.tv_usec = 0;
        setsockopt(client->sock_fd, SOL_SOCKET, SO_RCVTIMEO,
            &tv, sizeof(tv));

        from_len = sizeof(from_addr);
        len = recvfrom(client->sock_fd, buf, sizeof(buf), 0,
            (struct sockaddr*)&from_addr, &from_len);

        if (len > 0) {
            if (memcmp(buf, TAG_DMRD, 4) == 0) {
                parse_dmr_data(client, buf, len);
            }
            /* DMRP = pong, DMRB = beacon request, ignore */
        }

        /* 心跳 */
        uint64_t now = get_time_ms();
        if (now - client->last_ping_ms >= PING_INTERVAL_MS) {
            if (mmdvm_send_ping(client) == 0) {
                client->last_ping_ms = now;
            }
        }

        /* 自动重连: 如果长时间没收到数据且地址有效，重新解析+连接 */
        if (client->state == MMDVM_STATE_CONNECTED &&
            now - client->last_rx_ms > RECONNECT_INTERVAL_MS * 4) {
            /* 可能网络断了，尝试重新解析地址（DNS可能变了） */
            syslog(LOG_WARNING, "[mmdvm] no data for %lus, reconnecting\n",
                (unsigned long)((now - client->last_rx_ms) / 1000));
            client->state = MMDVM_STATE_CONNECTING;
            /* 重新解析在下面处理 */
        }

        /* 连接中：创建socket + 解析地址 */
        if (client->state == MMDVM_STATE_CONNECTING) {
            struct addrinfo hints, *res;
            char port_str[16];

            /* 创建 UDP socket (如果还没创建) */
            if (client->sock_fd < 0) {
                client->sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                if (client->sock_fd < 0) {
                    usleep(2000000);
                    continue;
                }
            }

            memset(&hints, 0, sizeof(hints));
            hints.ai_family   = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            snprintf(port_str, sizeof(port_str), "%u", client->config.master_port);

            if (getaddrinfo(client->config.master_addr, port_str,
                &hints, &res) == 0 && res) {
                struct sockaddr_in* sin = (struct sockaddr_in*)res->ai_addr;
                memcpy(&client->master_sin, sin, sizeof(struct sockaddr_in));
                client->addr_valid = true;
                freeaddrinfo(res);

                client->state = MMDVM_STATE_CONNECTED;
                client->last_ping_ms = now;
                client->last_rx_ms = now;
                syslog(LOG_INFO, "[mmdvm] connected to %s:%u\n",
                    client->config.master_addr, client->config.master_port);
            } else {
                usleep(2000000);
            }
        }

        /* 定期保存通联日志 (每30秒) */
        {
            static uint64_t last_log_save = 0;
            if (g_dmr_log.dirty && now - last_log_save > 30000) {
                dmr_log_save();
                last_log_save = now;
            }
        }
    }

    return NULL;
}

/* ===== API 实现 ===== */

int mmdvm_init(mmdvm_client_t* client, const mmdvm_config_t* config)
{
    if (!client || !config) return -1;

    memset(client, 0, sizeof(mmdvm_client_t));
    memcpy(&client->config, config, sizeof(mmdvm_config_t));

    client->state       = MMDVM_STATE_DISCONNECTED;
    client->sock_fd     = -1;
    client->addr_valid  = false;
    client->ping_interval_ms = PING_INTERVAL_MS;

    pthread_mutex_init(&client->mutex, NULL);

    srand((unsigned int)get_time_ms());
    client->stream_id[0] = (uint32_t)rand() | 1;
    client->stream_id[1] = (uint32_t)rand() | 1;

    /* 初始化通联日志 */
    dmr_log_init(config->data_dir ? config->data_dir : "/data/daynote");

    return 0;
}

int mmdvm_connect(mmdvm_client_t* client)
{
    if (!client) return -1;
    if (client->state == MMDVM_STATE_CONNECTED) return 0;

    /* Socket creation and DNS resolution happen in the recv thread
     * to support async connection and automatic retry. */
    client->state = MMDVM_STATE_CONNECTING;
    syslog(LOG_INFO, "[mmdvm] connecting to %s:%u (async)\n",
        client->config.master_addr, client->config.master_port);
    return 0;
}

int mmdvm_update_config(mmdvm_client_t* client, const mmdvm_config_t* config)
{
    bool master_changed;
    bool id_changed;

    if (!client || !config) return -1;

    master_changed = (strcmp(client->config.master_addr,
        config->master_addr) != 0) ||
        (client->config.master_port != config->master_port);
    id_changed = (client->config.dmr_id != config->dmr_id);

    pthread_mutex_lock(&client->mutex);
    memcpy(&client->config, config, sizeof(mmdvm_config_t));
    pthread_mutex_unlock(&client->mutex);

    syslog(LOG_INFO, "[mmdvm] config updated: %s:%u ID=%u CALL=%s\n",
        client->config.master_addr, client->config.master_port,
        client->config.dmr_id, client->config.callsign);

    if (master_changed) {
        /* Close old socket and reconnect to new master */
        syslog(LOG_INFO, "[mmdvm] master changed, reconnecting\n");
        if (client->sock_fd >= 0) {
            close(client->sock_fd);
            client->sock_fd = -1;
        }
        client->addr_valid = false;
        client->state = MMDVM_STATE_CONNECTING;
    } else if (id_changed) {
        syslog(LOG_INFO, "[mmdvm] DMR ID changed (takes effect on reconnect)\n");
    }

    return 0;
}

void mmdvm_disconnect(mmdvm_client_t* client)
{
    if (!client) return;

    if (client->sock_fd >= 0) {
        close(client->sock_fd);
        client->sock_fd = -1;
    }
    client->addr_valid = false;
    client->state = MMDVM_STATE_DISCONNECTED;
}

int mmdvm_start(mmdvm_client_t* client)
{
    if (!client) return -1;
    if (client->state == MMDVM_STATE_DISCONNECTED) return -1;

    client->thread_running = true;

    /* Explicit 8KB stack: recv thread holds a 2KB datagram buffer plus
     * getaddrinfo/DNS on its stack; default pthread stack is too small. */
    {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 8192);
        if (pthread_create(&client->thread, &attr, mmdvm_recv_thread, client) != 0) {
            pthread_attr_destroy(&attr);
            client->thread_running = false;
            return -1;
        }
        pthread_attr_destroy(&attr);
    }
    return 0;
}

void mmdvm_stop(mmdvm_client_t* client)
{
    if (!client) return;
    client->thread_running = false;
    if (client->thread) {
        pthread_join(client->thread, NULL);
        client->thread = 0;
    }
    dmr_log_save();
}

int mmdvm_send_dmr(mmdvm_client_t* client, const uint8_t* data, uint8_t len,
                   uint8_t slot, uint8_t frame_type,
                   uint32_t src_id, uint32_t dst_id)
{
    uint8_t buf[HOMEBREW_DATA_LEN];

    if (!client || !data || client->state != MMDVM_STATE_CONNECTED) return -1;
    if (len > 33) len = 33;

    memset(buf, 0, sizeof(buf));
    buf[0] = 'D'; buf[1] = 'M'; buf[2] = 'R'; buf[3] = 'D';
    buf[4] = 0;

    buf[5] = (src_id >> 16) & 0xFF;
    buf[6] = (src_id >> 8) & 0xFF;
    buf[7] = src_id & 0xFF;

    buf[8]  = (dst_id >> 16) & 0xFF;
    buf[9]  = (dst_id >> 8) & 0xFF;
    buf[10] = dst_id & 0xFF;

    buf[11] = (client->config.dmr_id >> 24) & 0xFF;
    buf[12] = (client->config.dmr_id >> 16) & 0xFF;
    buf[13] = (client->config.dmr_id >> 8) & 0xFF;
    buf[14] = client->config.dmr_id & 0xFF;

    buf[15] = (slot == 1) ? CTRL_SLOT2 : 0;
    if (frame_type == DMR_FRAME_VOICE_SYNC) {
        buf[15] |= CTRL_VOICE_SYNC;
    } else if (frame_type != DMR_FRAME_VOICE) {
        buf[15] |= CTRL_DATA_SYNC | (frame_type & CTRL_TYPE_MASK);
    }

    uint32_t sid = client->stream_id[slot];
    buf[16] = (sid >> 24) & 0xFF;
    buf[17] = (sid >> 16) & 0xFF;
    buf[18] = (sid >> 8) & 0xFF;
    buf[19] = sid & 0xFF;

    memcpy(buf + 20, data, len);

    return mmdvm_send_raw(client, buf, HOMEBREW_DATA_LEN);
}

int mmdvm_send_activation(mmdvm_client_t* client, uint8_t slot, uint32_t dst_id)
{
    uint8_t voice_lc[33] = {0};
    return mmdvm_send_dmr(client, voice_lc, 33, slot,
        DMR_FRAME_VOICE_LC_HEAD, client->config.dmr_id, dst_id);
}

bool mmdvm_get_current_call(mmdvm_client_t* client, uint8_t slot, dmr_call_t* call)
{
    bool active;

    if (!client || !call || slot > 1) return false;

    pthread_mutex_lock(&client->mutex);
    active = client->current_call[slot].active;
    if (active) {
        memcpy(call, &client->current_call[slot], sizeof(dmr_call_t));
        /* 更新实时时长 */
        call->duration_ms = (uint32_t)(get_time_ms() -
            client->call_start_ms[slot]);
    }
    pthread_mutex_unlock(&client->mutex);
    return active;
}

int mmdvm_get_history(mmdvm_client_t* client, dmr_call_t* buf, int max_count)
{
    /* 从持久化日志读取 */
    return mmdvm_get_log(buf, max_count);
}

mmdvm_state_t mmdvm_get_state(mmdvm_client_t* client)
{
    return client ? client->state : MMDVM_STATE_DISCONNECTED;
}

const char* mmdvm_state_str(mmdvm_state_t state)
{
    switch (state) {
        case MMDVM_STATE_DISCONNECTED: return "Disconnected";
        case MMDVM_STATE_CONNECTING:   return "Connecting...";
        case MMDVM_STATE_LOGGING_IN:   return "Logging in...";
        case MMDVM_STATE_CONNECTED:    return "Connected";
        case MMDVM_STATE_ERROR:        return "Error";
        default:                       return "Unknown";
    }
}

void mmdvm_deinit(mmdvm_client_t* client)
{
    if (!client) return;
    mmdvm_stop(client);
    mmdvm_disconnect(client);
    pthread_mutex_destroy(&client->mutex);
}

/* ===== 全局客户端 ===== */

mmdvm_client_t* g_mmdvm = NULL;

void mmdvm_set_global(mmdvm_client_t* client) { g_mmdvm = client; }
mmdvm_client_t* mmdvm_get_global(void) { return g_mmdvm; }

/* ===== DMR ID 到呼号 ===== */

const char* dmr_id_to_callsign(uint32_t id)
{
    static char buf[16];

    switch (id) {
        case 9990:     return "REFLECTOR";
        case 9999:     return "BRIDGE";
        case 4600001:  return "BM-TEST";
        default:       break;
    }

    /* 中国 DMR ID 段: 460xxxxxx (9位) */
    if (id >= 460000000 && id <= 460999999) {
        snprintf(buf, sizeof(buf), "CN%u", id);
        return buf;
    }

    snprintf(buf, sizeof(buf), "ID%u", id);
    return buf;
}
