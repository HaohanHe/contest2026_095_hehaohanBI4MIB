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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#ifndef CONFIG_LV_USE_NUTTX_LIBUV
#include <unistd.h>
#else
#include <uv.h>
#endif

#include <lvgl/lvgl.h>

#ifdef CONFIG_LV_USE_FREETYPE
#include "lvgl/src/libs/freetype/lv_freetype.h"
#endif

#include "daynote_core.h"
#include "daynote_ui.h"
#include "daynote_mmdvm.h"
#include "daynote_asr.h"

#include "infra/network_manager.h"
#include "infra/config_store.h"
#include "core/message_bus.h"
#include "infra/http_proxy.h"
#include "llm/llm_proxy.h"
#include "llm/llm_router.h"

#ifdef CONFIG_AI_AGENT_BLE_GATT
#include "infra/ble_gatt.h"
#include "infra/ble_cmd_handler.h"
#include <bluetooth.h>
#include <bt_adapter.h>
#include <netutils/cJSON.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_DAYNOTE_DATA_DIR
#define CONFIG_DAYNOTE_DATA_DIR "/data/daynote"
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifdef CONFIG_AI_AGENT_BLE_GATT
/* BLE command wrapper: handles DayNote-specific commands (mmdvm_config,
 * llm_config) and passes everything else (wifi_config, ping, status)
 * to the official ai_agent handler. */

static void daynote_ble_recv(const uint8_t* data, uint16_t len, void* user_data)
{
    cJSON* root;
    cJSON* cmd_obj;
    const char* cmd;
    char buf[512];
    uint16_t copy_len;

    (void)user_data;

    if (!data || len == 0) return;

    copy_len = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
    memcpy(buf, data, copy_len);
    buf[copy_len] = '\0';

    root = cJSON_Parse(buf);
    if (!root) {
        ble_cmd_handler_recv(data, len, user_data);
        return;
    }

    cmd_obj = cJSON_GetObjectItem(root, "cmd");
    if (!cmd_obj || !cJSON_IsString(cmd_obj)) {
        cJSON_Delete(root);
        ble_cmd_handler_recv(data, len, user_data);
        return;
    }

    cmd = cmd_obj->valuestring;

    if (strcmp(cmd, "mmdvm_config") == 0) {
        cJSON* f;
        char num_buf[32];
        mmdvm_client_t* mmdvm = mmdvm_get_global();
        mmdvm_config_t new_cfg;

        /* Start with current config if available */
        if (mmdvm) {
            memcpy(&new_cfg, &mmdvm->config, sizeof(new_cfg));
        } else {
            memset(&new_cfg, 0, sizeof(new_cfg));
        }

        f = cJSON_GetObjectItem(root, "dmr_id");
        if (f && cJSON_IsNumber(f)) {
            new_cfg.dmr_id = (uint32_t)f->valuedouble;
            snprintf(num_buf, sizeof(num_buf), "%u", new_cfg.dmr_id);
            claw_config_set("mmdvm_dmr_id", num_buf);
        }

        f = cJSON_GetObjectItem(root, "callsign");
        if (f && cJSON_IsString(f) && f->valuestring) {
            strncpy(new_cfg.callsign, f->valuestring,
                MMDVM_CALLSIGN_LEN - 1);
            claw_config_set("mmdvm_callsign", f->valuestring);
        }

        f = cJSON_GetObjectItem(root, "master");
        if (f && cJSON_IsString(f) && f->valuestring) {
            strncpy(new_cfg.master_addr, f->valuestring,
                MMDVM_MASTER_LEN - 1);
            claw_config_set("mmdvm_master", f->valuestring);
        }

        f = cJSON_GetObjectItem(root, "password");
        if (f && cJSON_IsString(f) && f->valuestring) {
            strncpy(new_cfg.password, f->valuestring,
                MMDVM_PASSWORD_LEN - 1);
            claw_config_set("mmdvm_password", f->valuestring);
        }

        if (mmdvm) {
            mmdvm_update_config(mmdvm, &new_cfg);
        }

        cJSON_Delete(root);
        return;
    }

    if (strcmp(cmd, "llm_config") == 0) {
        cJSON* f;
        llm_backend_t backend;

        memset(&backend, 0, sizeof(backend));
        strncpy(backend.host, "api.siliconflow.cn",
            sizeof(backend.host) - 1);
        strncpy(backend.path, "/v1/chat/completions",
            sizeof(backend.path) - 1);
        strncpy(backend.port, "443", sizeof(backend.port) - 1);
        strncpy(backend.model, "Qwen/Qwen2.5-7B-Instruct",
            sizeof(backend.model) - 1);
        backend.priority  = 0;
        backend.cost_tier = 1;
        backend.enabled   = true;

        f = cJSON_GetObjectItem(root, "host");
        if (f && cJSON_IsString(f) && f->valuestring)
            strncpy(backend.host, f->valuestring,
                sizeof(backend.host) - 1);

        f = cJSON_GetObjectItem(root, "model");
        if (f && cJSON_IsString(f) && f->valuestring)
            strncpy(backend.model, f->valuestring,
                sizeof(backend.model) - 1);

        f = cJSON_GetObjectItem(root, "api_key");
        if (f && cJSON_IsString(f) && f->valuestring)
            strncpy(backend.api_key, f->valuestring,
                sizeof(backend.api_key) - 1);

        f = cJSON_GetObjectItem(root, "path");
        if (f && cJSON_IsString(f) && f->valuestring)
            strncpy(backend.path, f->valuestring,
                sizeof(backend.path) - 1);

        llm_router_set_backend(0, &backend);
        llm_router_apply(0);
        syslog(LOG_INFO, "DayNote: LLM=%s/%s\n",
            backend.host, backend.model);

        /* Also accept ASR API key in same command */
        f = cJSON_GetObjectItem(root, "asr_api_key");
        if (f && cJSON_IsString(f) && f->valuestring) {
            claw_config_set("asr_api_key", f->valuestring);
            syslog(LOG_INFO, "DayNote: ASR key configured\n");
        }

        cJSON_Delete(root);
        return;
    }

    cJSON_Delete(root);
    ble_cmd_handler_recv(data, len, user_data);
}
#endif /* CONFIG_AI_AGENT_BLE_GATT */

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
static void lv_nuttx_uv_loop(uv_loop_t* loop, lv_nuttx_result_t* result)
{
    lv_nuttx_uv_t uv_info;
    void* data;

    uv_loop_init(loop);

    lv_memset(&uv_info, 0, sizeof(uv_info));
    uv_info.loop = loop;
    uv_info.disp = result->disp;
    uv_info.indev = result->indev;
#ifdef CONFIG_UINPUT_TOUCH
    uv_info.uindev = result->utouch_indev;
#endif

    data = lv_nuttx_uv_init(&uv_info);
    uv_run(loop, UV_RUN_DEFAULT);
    lv_nuttx_uv_deinit(&data);
}
#else
static void lv_nuttx_loop(void)
{
    while (1) {
        uint32_t idle;

        idle = lv_timer_handler();
        idle = idle ? idle : 1;
        usleep(idle * 1000);
    }
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char* argv[])
{
    lv_nuttx_dsc_t info;
    lv_nuttx_result_t result;
#ifdef CONFIG_LV_USE_NUTTX_LIBUV
    uv_loop_t ui_loop;
#endif
    char selftest_flag_path[128];
    bool ptt_selftest = false;
    int ret;
    int i;

    syslog(LOG_INFO, "DayNote starting\n");

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ptt-selftest") == 0 || strcmp(argv[i], "ptt-selftest") == 0 || strcmp(argv[i], "selftest") == 0) {
            ptt_selftest = true;
            syslog(LOG_INFO, "DayNote: PTT selftest requested via '%s'\n",
                argv[i]);
        }
    }

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
    lv_memset(&ui_loop, 0, sizeof(uv_loop_t));
#endif

    if (lv_is_initialized()) {
        syslog(LOG_ERR, "DayNote: LVGL already initialized\n");
        return -1;
    }

    lv_init();

    lv_nuttx_dsc_init(&info);

    /* Match luncher_mini initialization exactly */

#ifdef CONFIG_LV_USE_NUTTX_LCD
    info.fb_path = "/dev/lcd0";
#endif

    lv_nuttx_init(&info, &result);

    if (result.disp == NULL) {
        syslog(LOG_ERR, "DayNote: display init failed\n");
        lv_deinit();
        return -1;
    }

    /* Load custom TTF font via FreeType for Chinese support.
     * Searches multiple paths; falls back to LVGL default. */

#ifdef CONFIG_LV_USE_FREETYPE
    {
        static const char* font_paths[] = {
            "/data/daynote/font.ttf",
            "/system/fonts/NotoSansSC.ttf",
            "/etc/fonts/font.ttf",
            NULL
        };
        lv_font_t* custom_font = NULL;
        int fi;

        lv_freetype_init(64);

        for (fi = 0; font_paths[fi]; fi++) {
            custom_font = lv_freetype_font_create(font_paths[fi],
                LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 20,
                LV_FREETYPE_FONT_STYLE_NORMAL);
            if (custom_font) {
                lv_display_set_default_font(result.disp, custom_font);
                syslog(LOG_INFO, "DayNote: loaded font from %s\n",
                    font_paths[fi]);
                break;
            }
        }

        if (!custom_font) {
            syslog(LOG_WARNING, "DayNote: no TTF font found, "
                "Chinese shows as boxes. Put font.ttf in %s\n",
                CONFIG_DAYNOTE_DATA_DIR);
        }
    }
#endif

    /* Config store MUST come first: agent/MMDVM/WiFi all read persisted
     * settings (API keys, credentials) from it during their own init. */
    config_store_init();

    /* ai_agent core infrastructure is required BEFORE memo_agent_init(),
     * otherwise velaclaw_client_open fails and LLM classification silently
     * degrades to local keyword matching. */
    message_bus_init();
    http_proxy_init();
    llm_proxy_init();
    llm_router_init();
    syslog(LOG_INFO, "DayNote: ai_agent core initialized\n");

    ret = memo_store_init(CONFIG_DAYNOTE_DATA_DIR);
    if (ret < 0) {
        syslog(LOG_ERR, "DayNote: store init failed: %d\n", ret);
        lv_nuttx_deinit(&result);
        lv_deinit();
        return ret;
    }

    snprintf(selftest_flag_path, sizeof(selftest_flag_path),
        "%s/.ptt_selftest", CONFIG_DAYNOTE_DATA_DIR);
    if (!ptt_selftest && access(selftest_flag_path, F_OK) == 0) {
        ptt_selftest = true;
        syslog(LOG_INFO, "DayNote: PTT selftest requested via '%s'\n",
            selftest_flag_path);
    }

    /* Initialize AI agent (voice + LLM). Non-fatal if it fails. */

    ret = memo_agent_init();
    if (ret < 0) {
        syslog(LOG_WARNING, "DayNote: voice init failed: %d\n", ret);
    }

    /* Register SiliconFlow ASR backend (OpenAI-compatible).
     * Config via BLE: {"cmd":"llm_config","asr_api_key":"sk-..."}
     * or NSH config_store. Falls back to volc_asr if no key set. */
    daynote_asr_register();

    /* Start VAD auto-recording (always-on voice note capture).
     * Non-fatal: PTT manual recording still works if VAD fails. */
    ret = memo_vad_start();
    if (ret < 0) {
        syslog(LOG_WARNING, "DayNote: VAD start failed: %d\n", ret);
    }

    /* WiFi auto-reconnect using saved credentials (from BLE/NSH provisioning).
     * Non-fatal: if no credentials or WiFi unavailable, BLE provisioning starts. */
    ret = network_wifi_reconnect();
    if (ret == 0) {
        syslog(LOG_INFO, "DayNote: WiFi reconnected, IP=%s\n",
            network_get_ip());
    } else {
        syslog(LOG_INFO, "DayNote: no saved WiFi, waiting for BLE provisioning\n");
    }

    /* Start BLE GATT provisioning service (Xiaomi official phone pairing).
     * Phone App connects to "DayNote" BLE device, sends:
     *   {"cmd":"wifi_config","ssid":"...","password":"..."}
     *   {"cmd":"mmdvm_config","dmr_id":...,"callsign":"..."}
     *   {"cmd":"llm_config","api_key":"...","model":"..."}
     * Device saves credentials and connects. Non-fatal if BLE unavailable. */
#ifdef CONFIG_AI_AGENT_BLE_GATT
    {
        ble_gatt_config_t ble_cfg;
        bt_instance_t* bt_ins;
        int ble_retry;

        /* Ensure BLE adapter is enabled */
        bt_ins = bluetooth_get_instance();
        if (bt_ins) {
            bt_adapter_state_t state = bt_adapter_get_state(bt_ins);
            if (state < BT_ADAPTER_STATE_BLE_ON) {
                syslog(LOG_INFO, "DayNote: enabling BLE adapter...\n");
                bt_adapter_enable_le(bt_ins);
                usleep(2000000);
            }
        }

        memset(&ble_cfg, 0, sizeof(ble_cfg));
        ble_cfg.device_name = "DayNote";
        ble_cfg.recv_cb     = daynote_ble_recv;
        ble_cfg.user_data   = NULL;

        for (ble_retry = 0; ble_retry < 3; ble_retry++) {
            ret = ble_gatt_init(&ble_cfg);
            if (ret == 0) break;
            syslog(LOG_WARNING, "DayNote: BLE init retry %d (rc=%d)\n",
                ble_retry + 1, ret);
            usleep(3000000);
        }

        if (ret == 0) {
            syslog(LOG_INFO, "DayNote: BLE GATT started (DayNote)\n");
        } else {
            syslog(LOG_WARNING, "DayNote: BLE GATT init failed: %d\n", ret);
        }
    }
#endif

    /* Initialize MMDVM DMR hotspot client (stage 1: network only).
     * Connects to BrandMeister via Wi-Fi. Non-fatal if network unavailable.
     * Config loaded from persistent storage (set via BLE/NSH). */
    {
        static mmdvm_client_t s_mmdvm_client;
        mmdvm_config_t mmdvm_cfg;
        char cfg_buf[128];

        memset(&mmdvm_cfg, 0, sizeof(mmdvm_cfg));

        /* Load config from persistent storage, fall back to defaults */
        if (claw_config_get("mmdvm_master", cfg_buf, sizeof(cfg_buf)) > 0) {
            strncpy(mmdvm_cfg.master_addr, cfg_buf, MMDVM_MASTER_LEN - 1);
        } else {
            strncpy(mmdvm_cfg.master_addr, "3101.brandmeister.network",
                    MMDVM_MASTER_LEN - 1);
        }

        if (claw_config_get("mmdvm_callsign", cfg_buf, sizeof(cfg_buf)) > 0) {
            strncpy(mmdvm_cfg.callsign, cfg_buf, MMDVM_CALLSIGN_LEN - 1);
        } else {
            strncpy(mmdvm_cfg.callsign, "DAYNOTE", MMDVM_CALLSIGN_LEN - 1);
        }

        if (claw_config_get("mmdvm_password", cfg_buf, sizeof(cfg_buf)) > 0) {
            strncpy(mmdvm_cfg.password, cfg_buf, MMDVM_PASSWORD_LEN - 1);
        }

        if (claw_config_get("mmdvm_dmr_id", cfg_buf, sizeof(cfg_buf)) > 0) {
            mmdvm_cfg.dmr_id = (uint32_t)strtoul(cfg_buf, NULL, 10);
        }
        if (mmdvm_cfg.dmr_id == 0) {
            mmdvm_cfg.dmr_id = 460000000;  /* TODO: set your DMR ID */
        }

        mmdvm_cfg.master_port    = MMDVM_PORT_DEFAULT;
        mmdvm_cfg.data_dir       = CONFIG_DAYNOTE_DATA_DIR;
        mmdvm_cfg.slot1_enabled  = true;
        mmdvm_cfg.slot2_enabled  = true;
        mmdvm_cfg.duplex         = false;
        mmdvm_cfg.rx_freq        = 435000000;
        mmdvm_cfg.tx_freq        = 435000000;
        mmdvm_cfg.color_code     = 1;

        ret = mmdvm_init(&s_mmdvm_client, &mmdvm_cfg);
        if (ret == 0) {
            mmdvm_set_global(&s_mmdvm_client);
            ret = mmdvm_connect(&s_mmdvm_client);
            if (ret == 0) {
                mmdvm_start(&s_mmdvm_client);
                syslog(LOG_INFO, "DayNote: MMDVM DMR client started, "
                    "master=%s id=%u\n",
                    mmdvm_cfg.master_addr, mmdvm_cfg.dmr_id);
            } else {
                syslog(LOG_WARNING, "DayNote: MMDVM connect failed (no Wi-Fi?)\n");
            }
        }
    }

    ret = memo_ui_init();
    if (ret < 0) {
        syslog(LOG_ERR, "DayNote: UI init failed: %d\n", ret);
        memo_store_deinit();
        lv_nuttx_deinit(&result);
        lv_deinit();
        return ret;
    }

    if (ptt_selftest) {
        ret = memo_ui_start_ptt_selftest(1500);
        if (ret < 0) {
            syslog(LOG_ERR, "DayNote: PTT selftest scheduling failed: %d\n", ret);
        }
    }

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
    syslog(LOG_INFO, "DayNote: entering libuv event loop\n");
    lv_nuttx_uv_loop(&ui_loop, &result);
    syslog(LOG_INFO, "DayNote: libuv loop exited\n");
#else
    syslog(LOG_INFO, "DayNote: entering poll loop, indev=%p disp=%p\n",
        result.indev, result.disp);
    syslog(LOG_INFO, "DayNote: display res=%ldx%ld\n",
        (long)lv_display_get_horizontal_resolution(result.disp),
        (long)lv_display_get_vertical_resolution(result.disp));

    /* Ensure indev is in timer (poll) mode with 30ms period */
    if (result.indev) {
        lv_indev_set_mode(result.indev, LV_INDEV_MODE_TIMER);
        lv_timer_t* t = lv_indev_get_read_timer(result.indev);
        if (t) {
            lv_timer_set_period(t, 30);
            syslog(LOG_INFO, "DayNote: indev timer set to 30ms\n");
        } else {
            syslog(LOG_ERR, "DayNote: indev has no timer!\n");
        }
    } else {
        syslog(LOG_ERR, "DayNote: indev is NULL!\n");
    }

    lv_nuttx_loop();
#endif

    memo_ui_deinit();
    memo_agent_deinit();
    memo_store_deinit();
    lv_nuttx_deinit(&result);
    lv_deinit();

    syslog(LOG_INFO, "DayNote exiting\n");
    return 0;
}
