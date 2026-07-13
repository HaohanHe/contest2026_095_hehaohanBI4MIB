/****************************************************************************
 * SpaceLog 配置与配网模块实现
 *
 * 提供 LVGL 交互式设置页面，支持 WiFi/ASR/LLM/API Key/音频增益配置
 * 配置持久化到 /data/radio/config.ini (INI 格式)
 * WiFi 配置通过 wapi 命令连接，凭据存储在 /data/etc/WiFi/wapi.conf
 *
 * 参考：
 *   - ai_agent/config-wizard skill (配置流程)
 *   - ai_agent/ble_cmd_handler.c (WiFi 配网命令)
 *   - mini_memo/mini_memo_ui.c (LVGL 页面设计)
 *   - 现有 wifi_auto_connect.c (wapi 命令接口)
 ****************************************************************************/

#include "spacelog_settings.h"
#include "radio_config.h"
#include "config_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <syslog.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

/* ── 常量 ────────────────────────────────────────────────── */

#define WAPI_CONF_PATH     "/data/etc/WiFi/wapi.conf"
#define WIFI_IFACE         "wlan0"
#define MAX_CFG_VAL_LEN    128
#define MAX_LABEL_LEN      32
#define MAX_DISPLAY_VAL    40
#define SETTINGS_ITEM_H    32
#define SETTINGS_TITLE_H   24
#define KB_AREA_H          100
#define LIST_BOTTOM_PAD    8

/* 颜色（复用 main.c 风格） */
#define COL_BG          lv_color_hex(0x0a0e1a)
#define COL_BAR          lv_color_hex(0x0d1220)
#define COL_CARD         lv_color_hex(0x141c30)
#define COL_BORDER       lv_color_hex(0x253048)
#define COL_ACCENT       lv_color_hex(0x00d4ff)
#define COL_TEXT         lv_color_hex(0xe0e8f0)
#define COL_TEXT_DIM     lv_color_hex(0x8899aa)
#define COL_GREEN        lv_color_hex(0x33dd66)
#define COL_AMBER        lv_color_hex(0xffaa00)
#define COL_RED          lv_color_hex(0xff3344)

/* ── 配置项 key 定义 ─────────────────────────────────────── */

#define CFG_KEY_WIFI_SSID    "wifi_ssid"
#define CFG_KEY_WIFI_PSK     "wifi_psk"
#define CFG_KEY_API_KEY      "api_key"
#define CFG_KEY_ASR_MODEL    "asr_model"
#define CFG_KEY_LLM_MODEL    "llm_model"
#define CFG_KEY_AUDIO_GAIN   "audio_gain"

/* ── SELECT 选项数组 ─────────────────────────────────────── */

static const char *const ASR_MODEL_OPTS[] = {
    "FunAudioLLM/SenseVoiceSmall",
    "Paraformer/v2",
};
static const int ASR_MODEL_NUM = 2;

static const char *const LLM_MODEL_OPTS[] = {
    "Qwen/Qwen2.5-7B-Instruct",
    "deepseek-ai/DeepSeek-V3",
    "MiMo-7B-Base",
};
static const int LLM_MODEL_NUM = 3;

static const char *const GAIN_OPTS[] = {
    "Low",
    "Normal",
    "High",
};
static const int GAIN_NUM = 3;

/* ── 变更回调 ────────────────────────────────────────────── */

static void on_wifi_changed(const char *key, const char *val);
static void on_api_key_changed(const char *key, const char *val);
static void on_asr_model_changed(const char *key, const char *val);
static void on_llm_model_changed(const char *key, const char *val);

/* ── 配置项表 ────────────────────────────────────────────── */

static const spacelog_cfg_item_t s_default_items[] = {
    {
        .key = CFG_KEY_WIFI_SSID,
        .label = "WiFi SSID",
        .type = CFG_TYPE_TEXT,
        .default_val = "",
        .options = NULL,
        .num_options = 0,
        .on_change = on_wifi_changed,
    },
    {
        .key = CFG_KEY_WIFI_PSK,
        .label = "WiFi Password",
        .type = CFG_TYPE_PASSWORD,
        .default_val = "",
        .options = NULL,
        .num_options = 0,
        .on_change = on_wifi_changed,
    },
    {
        .key = CFG_KEY_API_KEY,
        .label = "SF API Key",
        .type = CFG_TYPE_PASSWORD,
        .default_val = "",
        .options = NULL,
        .num_options = 0,
        .on_change = on_api_key_changed,
    },
    {
        .key = CFG_KEY_ASR_MODEL,
        .label = "ASR Model",
        .type = CFG_TYPE_SELECT,
        .default_val = "FunAudioLLM/SenseVoiceSmall",
        .options = ASR_MODEL_OPTS,
        .num_options = ASR_MODEL_NUM,
        .on_change = on_asr_model_changed,
    },
    {
        .key = CFG_KEY_LLM_MODEL,
        .label = "LLM Model",
        .type = CFG_TYPE_SELECT,
        .default_val = "Qwen/Qwen2.5-7B-Instruct",
        .options = LLM_MODEL_OPTS,
        .num_options = LLM_MODEL_NUM,
        .on_change = on_llm_model_changed,
    },
    {
        .key = CFG_KEY_AUDIO_GAIN,
        .label = "Audio Gain",
        .type = CFG_TYPE_SELECT,
        .default_val = "Normal",
        .options = GAIN_OPTS,
        .num_options = GAIN_NUM,
        .on_change = NULL,
    },
    {
        .key = "device_info",
        .label = "Device Info",
        .type = CFG_TYPE_INFO,
        .default_val = "",
        .options = NULL,
        .num_options = 0,
        .on_change = NULL,
    },
};

#define NUM_CFG_ITEMS  (int)(sizeof(s_default_items) / sizeof(s_default_items[0]))

/* ── 模块状态 ────────────────────────────────────────────── */

static spacelog_settings_ctx_t s_ctx;
static ai_config_t s_ai_config;   /* 缓存的 AI 配置 */
static char s_wifi_ssid[128];
static char s_wifi_psk[128];
static char s_audio_gain[16];
static bool s_initialized = false;

/* ── 工具函数 ────────────────────────────────────────────── */

static void trim(char *s)
{
    char *start = s;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')
        start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
}

/* 从 INI 配置文件读取指定 key 的值 */
static int ini_get_value(const char *path, const char *key,
                         char *buf, size_t buf_size)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *k = line;
        char *v = eq + 1;
        trim(k);
        trim(v);
        if (strcmp(k, key) == 0) {
            snprintf(buf, buf_size, "%s", v);
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return -1;
}

/* 写入或更新 INI 配置文件中的 key=value */
static int ini_set_value(const char *path, const char *key, const char *val)
{
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *out = fopen(tmp_path, "w");
    if (!out) return -1;

    FILE *in = fopen(path, "r");
    bool found = false;

    if (in) {
        char line[512];
        while (fgets(line, sizeof(line), in)) {
            char tmp[512];
            snprintf(tmp, sizeof(tmp), "%s", line);
            char *eq = strchr(tmp, '=');
            if (eq) {
                *eq = '\0';
                char *k = tmp;
                trim(k);
                if (strcmp(k, key) == 0) {
                    fprintf(out, "%s=%s\n", key, val);
                    found = true;
                    continue;
                }
            }
            fputs(line, out);
        }
        fclose(in);
    }

    if (!found) {
        fprintf(out, "%s=%s\n", key, val);
    }

    fclose(out);
    rename(tmp_path, path);
    return 0;
}

/* 读取 WiFi 凭据 */
static void load_wifi_config(void)
{
    s_wifi_ssid[0] = '\0';
    s_wifi_psk[0] = '\0';

    FILE *fp = fopen(WAPI_CONF_PATH, "r");
    if (!fp) return;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        /* wapi.conf 格式: ssid=xxx 或 psk=xxx */
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *k = line;
        char *v = eq + 1;
        trim(k);
        trim(v);
        if (strcmp(k, "ssid") == 0) {
            snprintf(s_wifi_ssid, sizeof(s_wifi_ssid), "%s", v);
        } else if (strcmp(k, "psk") == 0) {
            snprintf(s_wifi_psk, sizeof(s_wifi_psk), "%s", v);
        }
    }
    fclose(fp);
}

/* 写入 WiFi 凭据到 wapi.conf */
static int save_wifi_config(const char *ssid, const char *psk)
{
    /* 确保目录存在 */
    mkdir("/data/etc", 0755);
    mkdir("/data/etc/WiFi", 0755);

    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", WAPI_CONF_PATH);

    FILE *out = fopen(tmp_path, "w");
    if (!out) return -1;

    fprintf(out, "ssid=%s\n", ssid);
    fprintf(out, "psk=%s\n", psk);
    fprintf(out, "mode=2\n");
    fclose(out);

    rename(tmp_path, WAPI_CONF_PATH);
    return 0;
}

/* Shell 转义：防止命令注入 */
static void shell_escape(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j < dst_size - 1; i++) {
        if (src[i] == '\\' || src[i] == '"') {
            if (j < dst_size - 2) {
                dst[j++] = '\\';
            }
        }
        dst[j++] = src[i];
    }
    dst[j] = '\0';
}

/* 通过 shell 命令连接 WiFi（复用 wifi_auto_connect 的 wapi 流程） */
static int wifi_connect(const char *ssid, const char *psk)
{
    char cmd[512];
    char ssid_escaped[128];
    char psk_escaped[128];
    int ret;

    shell_escape(ssid, ssid_escaped, sizeof(ssid_escaped));
    shell_escape(psk, psk_escaped, sizeof(psk_escaped));

    ret = system("wapi mode " WIFI_IFACE " 2");
    if (ret != 0) return -1;

    ret = system("wapi scan " WIFI_IFACE);
    if (ret != 0) return -1;

    snprintf(cmd, sizeof(cmd), "wapi psk " WIFI_IFACE " \"%s\" 3", psk_escaped);
    ret = system(cmd);
    if (ret != 0) return -1;

    snprintf(cmd, sizeof(cmd), "wapi essid " WIFI_IFACE " \"%s\" 1", ssid_escaped);
    ret = system(cmd);
    if (ret != 0) return -1;

    ret = system("renew " WIFI_IFACE);
    return ret;
}

/* 获取设备信息字符串 */
static void build_device_info(char *buf, size_t buf_size)
{
    char ip[64] = "N/A";
    unsigned long free_kb = 0, total_kb = 0;
    struct statvfs vfs;

    /* 读取存储使用情况 */
    if (statvfs("/data", &vfs) == 0) {
        total_kb = (unsigned long)(vfs.f_blocks * vfs.f_frsize / 1024);
        free_kb = (unsigned long)(vfs.f_bfree * vfs.f_frsize / 1024);
    }

    /* 读取 IP 地址（通过 NuttX getifaddrs） */
    {
        struct ifaddrs *iflist = NULL;
        if (getifaddrs(&iflist) == 0) {
            for (struct ifaddrs *ifa = iflist; ifa != NULL; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == NULL) continue;
                if (ifa->ifa_addr->sa_family != AF_INET) continue;
                if (strcmp(ifa->ifa_name, WIFI_IFACE) != 0) continue;
                struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
                inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
                break;
            }
            freeifaddrs(iflist);
        }
    }

    snprintf(buf, buf_size,
             "FW: %s\nIP: %s\nStorage: %luKB / %luKB\nASR: %s",
             RADIO_APP_VERSION,
             ip,
             total_kb - free_kb,
             total_kb,
             s_ai_config.asr_model);
}

/* ── 变更回调实现 ────────────────────────────────────────── */

static void on_wifi_changed(const char *key, const char *val)
{
    (void)key;
    (void)val;
    /* WiFi SSID 或密码变更后重新连接 */
    if (s_wifi_ssid[0] && s_wifi_psk[0]) {
        syslog(LOG_INFO, "[settings] WiFi reconnecting to %s\n", s_wifi_ssid);
        wifi_connect(s_wifi_ssid, s_wifi_psk);
    }
}

static void on_api_key_changed(const char *key, const char *val)
{
    (void)key;
    /* 写入 AI 配置并持久化 */
    snprintf(s_ai_config.api_key, sizeof(s_ai_config.api_key), "%s", val);
    s_ai_config.asr_enabled = (strlen(val) > 0);
    s_ai_config.llm_enabled = (strlen(val) > 0);
    radio_config_store_save(&s_ai_config);
}

static void on_asr_model_changed(const char *key, const char *val)
{
    (void)key;
    snprintf(s_ai_config.asr_model, sizeof(s_ai_config.asr_model), "%s", val);
    radio_config_store_save(&s_ai_config);
}

static void on_llm_model_changed(const char *key, const char *val)
{
    (void)key;
    snprintf(s_ai_config.llm_model, sizeof(s_ai_config.llm_model), "%s", val);
    radio_config_store_save(&s_ai_config);
}

/* ── Public API ──────────────────────────────────────────── */

int spacelog_settings_init(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.editing_idx = -1;

    /* 加载 AI 配置 */
    radio_config_store_load(&s_ai_config);

    /* 加载 WiFi 配置 */
    load_wifi_config();

    /* 加载音频增益（默认 Normal） */
    ini_get_value(CONFIG_PATH, CFG_KEY_AUDIO_GAIN,
                  s_audio_gain, sizeof(s_audio_gain));
    if (s_audio_gain[0] == '\0') {
        snprintf(s_audio_gain, sizeof(s_audio_gain), "Normal");
    }

    s_initialized = true;
    syslog(LOG_INFO, "[settings] initialized (WiFi SSID=%s, ASR=%s)\n",
           s_wifi_ssid[0] ? s_wifi_ssid : "none",
           s_ai_config.asr_model);
    return 0;
}

int spacelog_settings_get_value(const char *key, char *buf, size_t cap)
{
    if (!key || !buf || cap == 0) return -1;
    buf[0] = '\0';

    if (strcmp(key, CFG_KEY_WIFI_SSID) == 0) {
        snprintf(buf, cap, "%s", s_wifi_ssid);
        return 0;
    }
    if (strcmp(key, CFG_KEY_WIFI_PSK) == 0) {
        snprintf(buf, cap, "%s", s_wifi_psk);
        return 0;
    }
    if (strcmp(key, CFG_KEY_API_KEY) == 0) {
        snprintf(buf, cap, "%s", s_ai_config.api_key);
        return 0;
    }
    if (strcmp(key, CFG_KEY_ASR_MODEL) == 0) {
        snprintf(buf, cap, "%s", s_ai_config.asr_model);
        return 0;
    }
    if (strcmp(key, CFG_KEY_LLM_MODEL) == 0) {
        snprintf(buf, cap, "%s", s_ai_config.llm_model);
        return 0;
    }
    if (strcmp(key, CFG_KEY_AUDIO_GAIN) == 0) {
        snprintf(buf, cap, "%s", s_audio_gain);
        return 0;
    }
    if (strcmp(key, "device_info") == 0) {
        build_device_info(buf, cap);
        return 0;
    }
    return -1;
}

int spacelog_settings_set_value(const char *key, const char *val)
{
    if (!key || !val) return -1;
    int idx;

    for (idx = 0; idx < NUM_CFG_ITEMS; idx++) {
        if (strcmp(s_default_items[idx].key, key) == 0)
            break;
    }
    if (idx >= NUM_CFG_ITEMS) return -1;

    const spacelog_cfg_item_t *item = &s_default_items[idx];

    /* 更新内存值 */
    if (strcmp(key, CFG_KEY_WIFI_SSID) == 0) {
        snprintf(s_wifi_ssid, sizeof(s_wifi_ssid), "%s", val);
    } else if (strcmp(key, CFG_KEY_WIFI_PSK) == 0) {
        snprintf(s_wifi_psk, sizeof(s_wifi_psk), "%s", val);
    } else if (strcmp(key, CFG_KEY_API_KEY) == 0) {
        snprintf(s_ai_config.api_key, sizeof(s_ai_config.api_key), "%s", val);
        s_ai_config.asr_enabled = (strlen(val) > 0);
        s_ai_config.llm_enabled = (strlen(val) > 0);
        radio_config_store_save(&s_ai_config);
    } else if (strcmp(key, CFG_KEY_ASR_MODEL) == 0) {
        snprintf(s_ai_config.asr_model, sizeof(s_ai_config.asr_model), "%s", val);
        radio_config_store_save(&s_ai_config);
    } else if (strcmp(key, CFG_KEY_LLM_MODEL) == 0) {
        snprintf(s_ai_config.llm_model, sizeof(s_ai_config.llm_model), "%s", val);
        radio_config_store_save(&s_ai_config);
    } else if (strcmp(key, CFG_KEY_AUDIO_GAIN) == 0) {
        snprintf(s_audio_gain, sizeof(s_audio_gain), "%s", val);
        ini_set_value(CONFIG_PATH, CFG_KEY_AUDIO_GAIN, val);
    }

    /* 触发变更回调 */
    if (item->on_change) {
        item->on_change(key, val);
    }

    /* 如果是 WiFi 配置，同时写入 wapi.conf */
    if (strcmp(key, CFG_KEY_WIFI_SSID) == 0 || strcmp(key, CFG_KEY_WIFI_PSK) == 0) {
        save_wifi_config(s_wifi_ssid, s_wifi_psk);
    }

    return 0;
}

const spacelog_cfg_item_t *spacelog_settings_get_items(int *count)
{
    if (count) *count = NUM_CFG_ITEMS;
    return s_default_items;
}

bool spacelog_settings_is_visible(void)
{
    return s_ctx.visible;
}

/* ── LVGL UI 实现 ────────────────────────────────────────── */

/* 显示配置项值的简短文本（密码掩码） */
static void get_display_value(int idx, char *buf, size_t buf_size)
{
    const spacelog_cfg_item_t *item = &s_default_items[idx];
    char raw[MAX_CFG_VAL_LEN];

    if (item->type == CFG_TYPE_INFO) {
        build_device_info(buf, buf_size);
        return;
    }

    spacelog_settings_get_value(item->key, raw, sizeof(raw));

    if (item->type == CFG_TYPE_PASSWORD) {
        if (raw[0] == '\0') {
            snprintf(buf, buf_size, "未配置");
        } else {
            snprintf(buf, buf_size, "****");
        }
    } else if (raw[0] == '\0') {
        snprintf(buf, buf_size, "未配置");
    } else {
        /* 截断显示 */
        snprintf(buf, buf_size, "%s", raw);
        if (strlen(raw) > MAX_DISPLAY_VAL) {
            buf[MAX_DISPLAY_VAL - 3] = '.';
            buf[MAX_DISPLAY_VAL - 2] = '.';
            buf[MAX_DISPLAY_VAL - 1] = '\0';
        }
    }
}

/* 值标签数组，用于动态更新 */
static lv_obj_t *s_value_labels[NUM_CFG_ITEMS];

/* 键盘确认事件 */
static void kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        if (code == LV_EVENT_READY && s_ctx.editing_idx >= 0) {
            const char *txt = lv_textarea_get_text(s_ctx.ta);
            if (txt && txt[0]) {
                const spacelog_cfg_item_t *item = &s_default_items[s_ctx.editing_idx];
                spacelog_settings_set_value(item->key, txt);

                /* 更新显示 */
                if (s_value_labels[s_ctx.editing_idx]) {
                    char disp[MAX_DISPLAY_VAL + 1];
                    get_display_value(s_ctx.editing_idx, disp, sizeof(disp));
                    lv_label_set_text(s_value_labels[s_ctx.editing_idx], disp);
                }
            }
        }
        s_ctx.editing_idx = -1;
        if (s_ctx.kb) {
            lv_obj_del(s_ctx.kb);
            s_ctx.kb = NULL;
            s_ctx.ta = NULL;
            s_ctx.edit_label = NULL;
        }
    }
}

/* 弹出键盘编辑框 */
static void show_keyboard_editor(int idx)
{
    if (s_ctx.kb) {
        lv_obj_del(s_ctx.kb);
        s_ctx.kb = NULL;
    }

    s_ctx.editing_idx = idx;
    const spacelog_cfg_item_t *item = &s_default_items[idx];

    /* 创建覆盖层 */
    s_ctx.kb = lv_obj_create(s_ctx.overlay);
    lv_obj_set_size(s_ctx.kb, 320, 140);
    lv_obj_set_pos(s_ctx.kb, 0, 100);
    lv_obj_set_style_bg_color(s_ctx.kb, COL_BAR, 0);
    lv_obj_set_style_border_color(s_ctx.kb, COL_ACCENT, 0);
    lv_obj_set_style_border_width(s_ctx.kb, 1, 0);
    lv_obj_set_style_radius(s_ctx.kb, 0, 0);
    lv_obj_set_style_pad_all(s_ctx.kb, 4, 0);
    lv_obj_clear_flag(s_ctx.kb, LV_OBJ_FLAG_SCROLLABLE);

    /* 标签 */
    s_ctx.edit_label = lv_label_create(s_ctx.kb);
    lv_label_set_text(s_ctx.edit_label, item->label);
    lv_obj_set_style_text_color(s_ctx.edit_label, COL_ACCENT, 0);
    lv_obj_set_pos(s_ctx.edit_label, 4, 2);

    /* 文本输入框 */
    s_ctx.ta = lv_textarea_create(s_ctx.kb);
    lv_obj_set_size(s_ctx.ta, 312, 30);
    lv_obj_set_pos(s_ctx.ta, 2, 16);
    lv_textarea_set_placeholder_text(s_ctx.ta, item->label);
    lv_obj_set_style_bg_color(s_ctx.ta, COL_BG, 0);
    lv_obj_set_style_text_color(s_ctx.ta, COL_TEXT, 0);
    lv_obj_set_style_border_color(s_ctx.ta, COL_BORDER, 0);

    if (item->type == CFG_TYPE_PASSWORD) {
        lv_textarea_set_password_mode(s_ctx.ta, true);
    }

    /* 预填当前值 */
    char cur_val[MAX_CFG_VAL_LEN];
    spacelog_settings_get_value(item->key, cur_val, sizeof(cur_val));
    if (cur_val[0]) {
        lv_textarea_set_text(s_ctx.ta, cur_val);
    }

    /* 键盘 */
    lv_obj_t *kb = lv_keyboard_create(s_ctx.kb);
    lv_obj_set_size(kb, 312, 80);
    lv_obj_set_pos(kb, 2, 50);
    lv_keyboard_set_textarea(kb, s_ctx.ta);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_ALL, NULL);
}

/* SELECT 类型左右箭头切换 */
static void select_arrow_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);
    const spacelog_cfg_item_t *item = &s_default_items[idx];

    char cur_val[MAX_CFG_VAL_LEN];
    spacelog_settings_get_value(item->key, cur_val, sizeof(cur_val));

    int cur_opt = -1;
    for (int i = 0; i < item->num_options; i++) {
        if (strcmp(cur_val, item->options[i]) == 0) {
            cur_opt = i;
            break;
        }
    }
    if (cur_opt < 0) cur_opt = 0;

    /* 通过按钮位置判断左右 */
    lv_coord_t x = lv_obj_get_x(btn);
    int new_opt;
    if (x < 160) {
        /* 左箭头：上一个 */
        new_opt = (cur_opt - 1 + item->num_options) % item->num_options;
    } else {
        /* 右箭头：下一个 */
        new_opt = (cur_opt + 1) % item->num_options;
    }

    spacelog_settings_set_value(item->key, item->options[new_opt]);

    /* 更新显示 */
    if (s_value_labels[idx]) {
        char disp[MAX_DISPLAY_VAL + 1];
        get_display_value(idx, disp, sizeof(disp));
        lv_label_set_text(s_value_labels[idx], disp);
    }
}

/* 返回按钮回调 */
static void back_btn_cb(lv_event_t *e)
{
    (void)e;
    spacelog_settings_hide();
}

/* 行点击回调（TEXT/PASSWORD 类型项点击弹出键盘） */
static void row_click_cb(lv_event_t *e)
{
    /* 从 user_data 获取 index */
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    const spacelog_cfg_item_t *item = &s_default_items[idx];

    if (item->type == CFG_TYPE_TEXT || item->type == CFG_TYPE_PASSWORD) {
        show_keyboard_editor(idx);
    }
}

/* 重新创建行（修复：使用正确的回调注册） */
static void create_setting_row_v2(lv_obj_t *parent, int idx)
{
    const spacelog_cfg_item_t *item = &s_default_items[idx];
    int y = SETTINGS_TITLE_H + idx * SETTINGS_ITEM_H;

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_style_pad_all(row, 4, 0);
    lv_obj_set_style_radius(row, 2, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* 标签 */
    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, item->label);
    lv_obj_set_style_text_color(label, COL_TEXT_DIM, 0);
    lv_obj_set_pos(label, 4, 2);

    if (item->type == CFG_TYPE_INFO) {
        lv_obj_set_size(row, 312, 80);
        lv_obj_set_pos(row, 2, y);
        lv_obj_set_style_bg_color(row, COL_CARD, 0);
        lv_obj_set_style_border_color(row, COL_BORDER, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        s_value_labels[idx] = lv_label_create(row);
        char disp[256];
        get_display_value(idx, disp, sizeof(disp));
        lv_label_set_text(s_value_labels[idx], disp);
        lv_obj_set_style_text_color(s_value_labels[idx], COL_TEXT, 0);
        lv_obj_set_pos(s_value_labels[idx], 4, 14);
        lv_obj_set_width(s_value_labels[idx], 300);
        return;
    }

    if (item->type == CFG_TYPE_SELECT) {
        lv_obj_set_size(row, 312, SETTINGS_ITEM_H - 2);
        lv_obj_set_pos(row, 2, y);
        lv_obj_set_style_bg_color(row, COL_CARD, 0);
        lv_obj_set_style_border_color(row, COL_BORDER, 0);
        lv_obj_set_style_border_width(row, 1, 0);

        lv_obj_t *left_btn = lv_btn_create(row);
        lv_obj_set_size(left_btn, 20, 18);
        lv_obj_set_pos(left_btn, 120, 2);
        lv_obj_set_style_bg_color(left_btn, COL_BAR, 0);
        lv_obj_t *left_lbl = lv_label_create(left_btn);
        lv_label_set_text(left_lbl, "<");
        lv_obj_center(left_lbl);
        lv_obj_add_event_cb(left_btn, select_arrow_cb, LV_EVENT_CLICKED,
                           (void *)(intptr_t)idx);

        s_value_labels[idx] = lv_label_create(row);
        char disp[MAX_DISPLAY_VAL + 1];
        get_display_value(idx, disp, sizeof(disp));
        lv_label_set_text(s_value_labels[idx], disp);
        lv_obj_set_style_text_color(s_value_labels[idx], COL_ACCENT, 0);
        lv_obj_set_pos(s_value_labels[idx], 145, 4);

        lv_obj_t *right_btn = lv_btn_create(row);
        lv_obj_set_size(right_btn, 20, 18);
        lv_obj_set_pos(right_btn, 270, 2);
        lv_obj_set_style_bg_color(right_btn, COL_BAR, 0);
        lv_obj_t *right_lbl = lv_label_create(right_btn);
        lv_label_set_text(right_lbl, ">");
        lv_obj_center(right_lbl);
        lv_obj_add_event_cb(right_btn, select_arrow_cb, LV_EVENT_CLICKED,
                           (void *)(intptr_t)idx);
        return;
    }

    /* TEXT / PASSWORD */
    lv_obj_set_size(row, 312, SETTINGS_ITEM_H - 2);
    lv_obj_set_pos(row, 2, y);
    lv_obj_set_style_bg_color(row, COL_CARD, 0);
    lv_obj_set_style_border_color(row, COL_BORDER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

    s_value_labels[idx] = lv_label_create(row);
    char disp[MAX_DISPLAY_VAL + 1];
    get_display_value(idx, disp, sizeof(disp));
    lv_label_set_text(s_value_labels[idx], disp);
    lv_obj_set_style_text_color(s_value_labels[idx], COL_TEXT, 0);
    lv_obj_set_pos(s_value_labels[idx], 120, 4);

    lv_obj_add_event_cb(row, row_click_cb, LV_EVENT_CLICKED,
                       (void *)(intptr_t)idx);
}

/* 显式引用中文字体，防止链接器丢弃 */
extern const lv_font_t ai_radio_font;

int spacelog_settings_create(lv_obj_t *parent)
{
    if (!parent) return -1;

    /* 创建全屏覆盖层 */
    s_ctx.overlay = lv_obj_create(parent);
    lv_obj_set_size(s_ctx.overlay, 320, 240);
    lv_obj_set_pos(s_ctx.overlay, 0, 0);
    lv_obj_set_style_bg_color(s_ctx.overlay, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_ctx.overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_ctx.overlay, 0, 0);
    lv_obj_set_style_radius(s_ctx.overlay, 0, 0);
    lv_obj_set_style_border_width(s_ctx.overlay, 0, 0);
    lv_obj_set_style_text_font(s_ctx.overlay, &ai_radio_font, 0);
    lv_obj_clear_flag(s_ctx.overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题栏 */
    lv_obj_t *title = lv_label_create(s_ctx.overlay);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " Settings");
    lv_obj_set_style_text_color(title, COL_ACCENT, 0);
    lv_obj_set_pos(title, 8, 4);

    /* 返回按钮 */
    lv_obj_t *back_btn = lv_btn_create(s_ctx.overlay);
    lv_obj_set_size(back_btn, 50, 20);
    lv_obj_set_pos(back_btn, 260, 2);
    lv_obj_set_style_bg_color(back_btn, COL_CARD, 0);
    lv_obj_set_style_pad_all(back_btn, 0, 0);
    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, "Back");
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    /* 滚动列表容器 */
    s_ctx.list = lv_obj_create(s_ctx.overlay);
    lv_obj_set_size(s_ctx.list, 316, 214);
    lv_obj_set_pos(s_ctx.list, 2, SETTINGS_TITLE_H);
    lv_obj_set_style_bg_color(s_ctx.list, COL_BG, 0);
    lv_obj_set_style_border_width(s_ctx.list, 0, 0);
    lv_obj_set_style_pad_all(s_ctx.list, 2, 0);
    lv_obj_set_style_radius(s_ctx.list, 0, 0);
    lv_obj_set_flex_flow(s_ctx.list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_ctx.list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_ctx.list, 2, 0);

    /* 创建配置项行 */
    for (int i = 0; i < NUM_CFG_ITEMS; i++) {
        create_setting_row_v2(s_ctx.list, i);
    }

    /* 默认隐藏 */
    lv_obj_add_flag(s_ctx.overlay, LV_OBJ_FLAG_HIDDEN);
    s_ctx.visible = false;

    return 0;
}

void spacelog_settings_show(void)
{
    if (!s_ctx.overlay) return;

    /* 刷新设备信息 */
    if (s_value_labels[6]) {
        char disp[256];
        get_display_value(6, disp, sizeof(disp));
        lv_label_set_text(s_value_labels[6], disp);
    }

    lv_obj_clear_flag(s_ctx.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ctx.overlay);
    s_ctx.visible = true;
}

void spacelog_settings_hide(void)
{
    if (!s_ctx.overlay) return;

    /* 如果键盘打开，先关闭 */
    if (s_ctx.kb) {
        lv_obj_del(s_ctx.kb);
        s_ctx.kb = NULL;
        s_ctx.ta = NULL;
        s_ctx.edit_label = NULL;
        s_ctx.editing_idx = -1;
    }

    lv_obj_add_flag(s_ctx.overlay, LV_OBJ_FLAG_HIDDEN);
    s_ctx.visible = false;
}
