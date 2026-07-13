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

#include "channels/cmd_channel.h"
#include "infra/config_store.h"
#include "channels/feishu_bot.h"
#include "channels/mqtt_channel.h"
#ifdef CONFIG_AI_AGENT_NODE
#include "node/node_client.h"
#include "node/node_manager.h"
#endif
#include "ui/qrcode_display.h"
#include "agent_compat.h"
#include "agent_config.h"
#include "channels/weixin_channel.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

void cmd_set_feishu_app(int argc, char** argv)
{
    if (argc < 3) {
        printf("Usage: set_feishu_app <app_id> <app_secret>\n");
        return;
    }
    feishu_set_app(argv[1], argv[2]);
    printf("Feishu app credentials saved (app_id=%s).\n", argv[1]);
}

void cmd_set_feishu_user_token(int argc, char** argv)
{
    if (argc < 2) {
        printf("Usage: set_feishu_user_token <token>\n");
        return;
    }
    feishu_set_user_token(argv[1]);
    printf("Feishu user_access_token saved (%.8s...).\n", argv[1]);
}

void cmd_set_mqtt(int argc, char** argv)
{
    if (argc < 2) {
        printf("Usage: set_mqtt <host:port> [client_id]\n"
               "  e.g. set_mqtt <broker-host>:1883\n"
               "       set_mqtt <broker-host>:1883 my-device\n");
        return;
    }

    claw_config_set(AGENT_CFG_KEY_MQTT_BROKER, argv[1]);
    if (argc >= 3) {
        claw_config_set(AGENT_CFG_KEY_MQTT_CLIENT_ID, argv[2]);
    }

    /* Re-init and start immediately so no restart is needed */
    mqtt_channel_stop();
    mqtt_channel_init();
    mqtt_channel_start();
    printf("MQTT broker set to %s and started.\n", argv[1]);
}

#ifdef CONFIG_AI_AGENT_NODE
void cmd_node_list(void)
{
    char buf[1024];
    printf("=== Connected Nodes ===\n");
    node_manager_list(buf, sizeof(buf));
    printf("%s", buf);
    printf("=======================\n");
}
#endif

void cmd_set_gateway(int argc, char** argv)
{
    if (argc < 2) {
        printf("Usage: set_gateway <host> [port] [token]\n");
        printf("  e.g. set_gateway <host> 8080 my-token\n");
        return;
    }
    claw_config_set(AGENT_CFG_KEY_GATEWAY_HOST, argv[1]);
    if (argc >= 3)
        claw_config_set(AGENT_CFG_KEY_GATEWAY_PORT, argv[2]);
    if (argc >= 4)
        claw_config_set(AGENT_CFG_KEY_GATEWAY_TOKEN, argv[3]);
    printf("Gateway set to %s:%s%s. Use 'node_start' to connect.\n",
        argv[1], argc >= 3 ? argv[2] : "8080",
        argc >= 4 ? " (token saved)" : "");
}

#ifdef CONFIG_AI_AGENT_NODE
void cmd_node_start(void)
{
    int ret = node_client_start();
    if (ret == OK) {
        printf("Node client started (check logs for connection status).\n");
    } else {
        printf("Node client start failed.\n");
    }
}

void cmd_node_stop(void)
{
    node_client_stop();
    printf("Node client stopped.\n");
}
#endif

void cmd_set_weixin_token(int argc, char** argv)
{
    if (argc < 2) {
        printf("Usage: set_weixin_token <bot_token>\n");
        return;
    }
    weixin_channel_set_token(argv[1], 0);
    printf("WeChat bot token saved.\n");
}

void cmd_weixin_login(void)
{
    char qr_url[512] = "";
    char qrcode_id[128] = "";

    printf("Requesting QR code from iLink Bot API...\n");
    int rc = weixin_channel_login(qr_url, sizeof(qr_url),
        qrcode_id, sizeof(qrcode_id));
    if (rc != 0) {
        printf("Failed to get QR code (rc=%d). Check network.\n", rc);
        return;
    }

    printf("QR URL: %s\n", qr_url);
    claw_show_qrcode(qr_url);
    printf("Scan the QR code with WeChat, then waiting...\n");

    for (int i = 0; i < 60; i++) {
        sleep(2);
        int status = weixin_channel_poll_login(qrcode_id);
        if (status == 1) {
            printf("Login confirmed! Token saved.\n");
            return;
        } else if (status == 2) {
            printf("Scanned, waiting for confirm...\n");
        } else if (status == -3) {
            printf("QR code expired. Run weixin_login again.\n");
            return;
        } else if (status < 0) {
            printf("Poll error (rc=%d).\n", status);
            return;
        }
    }
    printf("Timeout waiting for scan.\n");
}
