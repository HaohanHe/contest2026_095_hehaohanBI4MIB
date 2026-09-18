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
 * daynote_mmdvm.h - MMDVM Homebrew DMR 网络客户端
 *
 * 纯 UDP 协议，通过 Wi-Fi 连接 BrandMeister 网络
 * 不需要射频硬件，可接收通话信息、发送短消息
 *
 * 基于 MMDVMHost DMRNetwork.cpp 协议实现
 */

#ifndef __DAYNOTE_MMDVM_H
#define __DAYNOTE_MMDVM_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== MMDVM Homebrew 协议常量 ===== */

#define MMDVM_PORT_DEFAULT       62031
#define MMDVM_BUF_SIZE           2048
#define MMDVM_CALLSIGN_LEN       16
#define MMDVM_MASTER_LEN         128
#define MMDVM_PASSWORD_LEN       64
#define MMDVM_MAX_HISTORY        20

/* 协议签名 */
#define MMDVM_SIG                0xE0U

/* 命令类型 (字节0) */
#define MMDVM_CMD_REPEATER       0x00U  /* 中继器数据 */
#define MMDVM_CMD_MASTER         0x01U  /* 主站数据 */
#define MMDVM_CMD_DMRD           0x10U  /* DMR 数据 */
#define MMDVM_CMD_DMRDACK        0x11U  /* DMR 数据确认 */
#define MMDVM_CMD_RPTLOGIN       0x00U  /* 登录 (在 repeater 标签下) */
#define MMDVM_CMD_RPTLOGINACK    0x01U  /* 登录确认 */
#define MMDVM_CMD_RPTLOGINNAK    0x02U  /* 登录拒绝 */
#define MMDVM_CMD_RPTCONFIG      0x03U  /* 配置请求 */
#define MMDVM_CMD_RPTCONFIGACK   0x04U  /* 配置确认 */
#define MMDVM_CMD_RPTCLOSE       0x05U  /* 关闭 */
#define MMDVM_CMD_RPTPING        0x06U  /* 心跳 */
#define MMDVM_CMD_RPTPONG        0x07U  /* 心跳回复 */
#define MMDVM_CMD_RPTACK         0x08U  /* 确认 */
#define MMDVM_CMD_RPTNAK         0x09U  /* 拒绝 */

/* DMR 帧类型 (DMRD 数据包内) */
#define DMR_FRAME_VOICE_SYNC     0xF0U
#define DMR_FRAME_VOICE          0xF1U
#define DMR_FRAME_VOICE_LC_HEAD  0x01U
#define DMR_FRAME_TERMINATOR     0x02U
#define DMR_FRAME_CSBK           0x03U
#define DMR_FRAME_DATA_HEADER    0x06U
#define DMR_FRAME_RATE_12_DATA   0x07U
#define DMR_FRAME_IDLE           0x09U

/* DMR 时隙 */
#define DMR_SLOT_1               0U
#define DMR_SLOT_2               1U

/* 通话类型 */
#define DMR_CALL_GROUP           0U  /* 组呼 */
#define DMR_CALL_PRIVATE         1U  /* 个呼 */
#define DMR_CALL_ALL             2U  /* 全呼 */

/* ===== 数据结构 ===== */

/**
 * DMR 通话信息
 */
typedef struct {
    uint32_t    src_id;          /* 源 DMR ID */
    uint32_t    dst_id;          /* 目标 DMR ID (TalkGroup 或 个呼ID) */
    uint8_t     slot;            /* 时隙 0=TS1, 1=TS2 */
    uint8_t     call_type;       /* 通话类型 */
    char        src_callsign[MMDVM_CALLSIGN_LEN];  /* 源呼号 */
    char        src_name[MMDVM_CALLSIGN_LEN];      /* 源姓名 */
    uint32_t    duration_ms;     /* 通话时长 (ms) */
    uint64_t    start_time;      /* 开始时间 (unix timestamp, 秒) */
    bool        active;          /* 是否正在进行 */
    uint8_t     frame_count;     /* 收到的帧数 */
    uint8_t     ber;             /* 误码率 (0-255, /2.55 = %) */
    uint8_t     rssi;            /* 信号强度 */
} dmr_call_t;

/**
 * 通话历史记录
 */
typedef struct {
    dmr_call_t  calls[MMDVM_MAX_HISTORY];
    int         count;
    int         head;
} dmr_history_t;

/**
 * MMDVM 客户端配置
 */
typedef struct {
    char        master_addr[MMDVM_MASTER_LEN];  /* BrandMeister master 地址 */
    uint16_t    master_port;                    /* master 端口 */
    uint32_t    dmr_id;                         /* 本热点 DMR ID */
    char        callsign[MMDVM_CALLSIGN_LEN];   /* 本热点呼号 */
    char        password[MMDVM_PASSWORD_LEN];   /* Hotspot Security 密码 */
    const char* data_dir;                       /* 数据目录 (通联日志) */
    bool        slot1_enabled;
    bool        slot2_enabled;
    bool        duplex;
    uint32_t    rx_freq;                        /* 接收频率 (Hz) */
    uint32_t    tx_freq;                        /* 发射频率 (Hz) */
    uint8_t     color_code;                     /* DMR 色码 */
} mmdvm_config_t;

/**
 * MMDVM 客户端状态
 */
typedef enum {
    MMDVM_STATE_DISCONNECTED = 0,
    MMDVM_STATE_CONNECTING,
    MMDVM_STATE_LOGGING_IN,
    MMDVM_STATE_CONNECTED,
    MMDVM_STATE_ERROR
} mmdvm_state_t;

/**
 * MMDVM 客户端
 */
typedef struct {
    mmdvm_config_t  config;
    mmdvm_state_t   state;
    int             sock_fd;
    pthread_t       thread;
    bool            thread_running;
    pthread_mutex_t mutex;

    /* 网络地址缓存 */
    struct sockaddr_in master_sin;
    bool            addr_valid;

    /* 通话状态 */
    dmr_call_t      current_call[2];  /* 每个时隙一个当前通话 */

    /* 协议状态 */
    uint32_t        stream_id[2];     /* 每个时隙的流ID */
    uint64_t        call_start_ms[2]; /* 每个时隙通话开始的 monotonic ms */
    uint64_t        last_ping_ms;
    uint64_t        last_rx_ms;
    uint32_t        ping_interval_ms;

    /* 统计 */
    uint32_t        tx_frames;
    uint32_t        rx_frames;
    uint32_t        errors;
} mmdvm_client_t;

/* ===== API ===== */

/**
 * 初始化 MMDVM 客户端
 */
int mmdvm_init(mmdvm_client_t *client, const mmdvm_config_t *config);

/**
 * 连接到 BrandMeister master
 */
int mmdvm_connect(mmdvm_client_t *client);

/**
 * Hot-reload configuration (e.g. after BLE provisioning).
 * If master address changed, reconnects automatically.
 */
int mmdvm_update_config(mmdvm_client_t *client, const mmdvm_config_t *config);

/**
 * 断开连接
 */
void mmdvm_disconnect(mmdvm_client_t *client);

/**
 * 启动接收线程
 */
int mmdvm_start(mmdvm_client_t *client);

/**
 * 停止接收线程
 */
void mmdvm_stop(mmdvm_client_t *client);

/**
 * 发送 DMR 数据帧 (需要射频前端才能实际发射)
 */
int mmdvm_send_dmr(mmdvm_client_t *client, const uint8_t *data, uint8_t len,
                   uint8_t slot, uint8_t frame_type, uint32_t src_id, uint32_t dst_id);

/**
 * 发送 PTT 激活 (短消息或数据)
 */
int mmdvm_send_activation(mmdvm_client_t *client, uint8_t slot, uint32_t dst_id);

/**
 * 获取当前通话信息
 */
bool mmdvm_get_current_call(mmdvm_client_t *client, uint8_t slot, dmr_call_t *call);

/**
 * 获取通话历史（从持久化日志）
 */
int mmdvm_get_history(mmdvm_client_t *client, dmr_call_t *buf, int max_count);

/**
 * 保存通联日志到文件
 */
int dmr_log_save(void);

/**
 * 加载通联日志
 */
int dmr_log_load(void);

/**
 * 获取通联日志条目
 */
int mmdvm_get_log(dmr_call_t *buf, int max_count);

/**
 * 获取连接状态
 */
mmdvm_state_t mmdvm_get_state(mmdvm_client_t *client);

/**
 * 获取状态字符串
 */
const char *mmdvm_state_str(mmdvm_state_t state);

/**
 * 清理资源
 */
void mmdvm_deinit(mmdvm_client_t *client);

/**
 * DMR ID 转呼号 (简单本地映射，后续可接 radioid.net API)
 */
const char *dmr_id_to_callsign(uint32_t id);

/* ===== 全局客户端 (UI 层访问) ===== */

extern mmdvm_client_t *g_mmdvm;

/** 设置全局客户端指针 (main 中调用) */
void mmdvm_set_global(mmdvm_client_t *client);

/** 获取全局客户端指针 */
mmdvm_client_t *mmdvm_get_global(void);

#ifdef __cplusplus
}
#endif

#endif /* __DAYNOTE_MMDVM_H */
