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

#include "tools/tool_feishu_chat.h"
#include "cJSON.h"
#include "channels/feishu_bot.h"
#ifdef CONFIG_AI_AGENT_NODE
#include "node/node_manager.h"
#endif
#include "agent_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* TAG = "tool_feishu_chat";

#define RESP_BUF_SIZE (16 * 1024)

/* ── feishu_chat_members ─────────────────────────────────────── */

int tool_feishu_chat_members_execute(const char* input_json, char* output,
    size_t output_size)
{
    cJSON* input = cJSON_Parse(input_json);
    if (!input) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ERROR;
    }

    const char* chat_id = cJSON_GetStringValue(cJSON_GetObjectItem(input, "chat_id"));
    if (!chat_id || !chat_id[0]) {
        snprintf(output, output_size, "Error: chat_id is required");
        cJSON_Delete(input);
        return ERROR;
    }

    /* GET /open-apis/im/v1/chats/{chat_id}/members?member_id_type=open_id */
    char path[256];
    snprintf(path, sizeof(path),
        "/open-apis/im/v1/chats/%s/members?member_id_type=open_id", chat_id);

    char* resp = malloc(RESP_BUF_SIZE);
    if (!resp) {
        snprintf(output, output_size, "Error: OOM");
        cJSON_Delete(input);
        return ERROR;
    }

    int status = feishu_api_request("GET", path, NULL, 0, resp, RESP_BUF_SIZE);
    cJSON_Delete(input);

    if (status != 200) {
        snprintf(output, output_size, "Error: Feishu API HTTP %d: %.200s", status,
            resp);
        free(resp);
        return ERROR;
    }

    /* Parse response:
     * { "code":0, "data":{ "items":[
     *     {"member_id":"ou_xxx","name":"nana","member_id_type":"open_id"}, ...
     * ] } }
     */
    cJSON* root = cJSON_Parse(resp);
    free(resp);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON from Feishu API");
        return ERROR;
    }

    cJSON* code = cJSON_GetObjectItem(root, "code");
    if (!cJSON_IsNumber(code) || code->valueint != 0) {
        cJSON* msg = cJSON_GetObjectItem(root, "msg");
        snprintf(output, output_size, "Error: Feishu API code=%d msg=%s",
            cJSON_IsNumber(code) ? (int)code->valueint : -1,
            (msg && cJSON_IsString(msg)) ? msg->valuestring : "unknown");
        cJSON_Delete(root);
        return ERROR;
    }

    cJSON* data = cJSON_GetObjectItem(root, "data");
    cJSON* items = data ? cJSON_GetObjectItem(data, "items") : NULL;

    /* Build simplified output: [{name, open_id}, ...] */
    cJSON* result = cJSON_CreateArray();
    if (items && cJSON_IsArray(items)) {
        cJSON* item;
        cJSON_ArrayForEach(item, items)
        {
            cJSON* name_j = cJSON_GetObjectItem(item, "name");
            cJSON* mid_j = cJSON_GetObjectItem(item, "member_id");
            if (cJSON_IsString(mid_j)) {
                cJSON* entry = cJSON_CreateObject();
                cJSON_AddStringToObject(
                    entry, "name", cJSON_IsString(name_j) ? name_j->valuestring : "");
                cJSON_AddStringToObject(entry, "open_id", mid_j->valuestring);
                cJSON_AddItemToArray(result, entry);
            }
        }
    }

    char* out_str = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    cJSON_Delete(root);

    if (out_str) {
        snprintf(output, output_size, "%s", out_str);
        free(out_str);
    } else {
        snprintf(output, output_size, "[]");
    }

    syslog(LOG_INFO, "[%s] chat_members for %s done\n", TAG, chat_id);
    return OK;
}

/* ── feishu_send_mention ─────────────────────────────────────── */

int tool_feishu_send_mention_execute(const char* input_json, char* output,
    size_t output_size)
{
    cJSON* input = cJSON_Parse(input_json);
    if (!input) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ERROR;
    }

    const char* chat_id_raw = cJSON_GetStringValue(cJSON_GetObjectItem(input, "chat_id"));
    const char* open_id_raw = cJSON_GetStringValue(cJSON_GetObjectItem(input, "open_id"));
    const char* name_raw = cJSON_GetStringValue(cJSON_GetObjectItem(input, "name"));
    const char* text_raw = cJSON_GetStringValue(cJSON_GetObjectItem(input, "text"));

    if (!chat_id_raw || !chat_id_raw[0]) {
        snprintf(output, output_size, "Error: chat_id is required");
        cJSON_Delete(input);
        return ERROR;
    }
    if (!open_id_raw || !open_id_raw[0]) {
        snprintf(output, output_size, "Error: open_id is required");
        cJSON_Delete(input);
        return ERROR;
    }
    if (!text_raw)
        text_raw = "";
    if (!name_raw || !name_raw[0])
        name_raw = "user";

    /* Copy values before freeing input cJSON */
    char* chat_id = strdup(chat_id_raw);
    char* open_id = strdup(open_id_raw);
    char* name = strdup(name_raw);
    char* text = strdup(text_raw);
    cJSON_Delete(input);

    if (!chat_id || !open_id || !name || !text) {
        snprintf(output, output_size, "Error: OOM");
        free(chat_id);
        free(open_id);
        free(name);
        free(text);
        return ERROR;
    }

    /* Feishu @mention format in text message:
     * content: {"text":"<at user_id=\"ou_xxx\">name</at> message text"}
     *
     * The <at> tag is a special Feishu rich-text marker that renders
     * as an @mention in the chat UI and sends a notification.
     */

    /* Build content string with @mention tag */
    cJSON* content_obj = cJSON_CreateObject();
    size_t mention_len = strlen(open_id) + strlen(name) + strlen(text) + 64;
    char* mention_text = malloc(mention_len);
    if (!mention_text) {
        snprintf(output, output_size, "Error: OOM");
        cJSON_Delete(content_obj);
        free(chat_id);
        free(open_id);
        free(name);
        free(text);
        return ERROR;
    }
    snprintf(mention_text, mention_len, "<at user_id=\"%s\">%s</at> %s",
             open_id, name, text);
    cJSON_AddStringToObject(content_obj, "text", mention_text);
    free(mention_text);

    char* content_str = cJSON_PrintUnformatted(content_obj);
    cJSON_Delete(content_obj);

    if (!content_str) {
        snprintf(output, output_size, "Error: OOM building content");
        free(chat_id);
        free(open_id);
        free(name);
        free(text);
        return ERROR;
    }

    /* Build request body */
    cJSON* body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "receive_id", chat_id);
    cJSON_AddStringToObject(body, "msg_type", "text");
    cJSON_AddStringToObject(body, "content", content_str);
    free(content_str);

    char* body_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    if (!body_str) {
        snprintf(output, output_size, "Error: OOM");
        free(chat_id);
        free(open_id);
        free(name);
        free(text);
        return ERROR;
    }

    char* resp = malloc(4096);
    if (!resp) {
        free(body_str);
        snprintf(output, output_size, "Error: OOM");
        free(chat_id);
        free(open_id);
        free(name);
        free(text);
        return ERROR;
    }

    syslog(LOG_INFO, "[%s] Sending @mention to %s in %s\n", TAG, open_id,
        chat_id);

    int status = feishu_api_post("/open-apis/im/v1/messages?receive_id_type=chat_id",
        body_str, resp, 4096);
    free(body_str);

    if (status != 200 && status != 201) {
        snprintf(output, output_size, "Error: send failed HTTP %d: %.200s", status,
            resp);
        free(resp);
        free(chat_id);
        free(open_id);
        free(name);
        free(text);
        return ERROR;
    }

    /* Parse response for message_id */
    cJSON* root = cJSON_Parse(resp);
    free(resp);

    if (root) {
        cJSON* code = cJSON_GetObjectItem(root, "code");
        if (cJSON_IsNumber(code) && code->valueint != 0) {
            cJSON* msg = cJSON_GetObjectItem(root, "msg");
            snprintf(output, output_size, "Error: code=%d msg=%s",
                (int)code->valueint,
                (msg && cJSON_IsString(msg)) ? msg->valuestring : "?");
            cJSON_Delete(root);
            free(chat_id);
            free(open_id);
            free(name);
            free(text);
            return ERROR;
        }
        cJSON_Delete(root);
    }

    snprintf(output, output_size,
        "@mention message sent successfully to %s in the chat. "
        "No further action needed for this mention.",
        name);
    syslog(LOG_INFO, "[%s] @mention message sent to %s\n", TAG, name);

    /* Forward the message to connected nodes so bot-to-bot @mentions
     * work even when the platform doesn't push events between bots. */
#ifdef CONFIG_AI_AGENT_NODE
    if (node_manager_active_count() > 0) {
        char fwd_text[512];
        snprintf(fwd_text, sizeof(fwd_text), "<at user_id=\"%s\">%s</at> %s",
            open_id, name, text);
        node_manager_broadcast_chat("feishu", chat_id, fwd_text);
    }
#endif

    free(chat_id);
    free(open_id);
    free(name);
    free(text);
    return OK;
}
