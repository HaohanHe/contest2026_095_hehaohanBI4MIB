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

#include "tools/mcp_bridge.h"
#include "agent_compat.h"

#include "cJSON.h"
#include "tools/mcp_builtin_tools.h"
#include "tools/mcp_tool_registry.h"

#include <stdlib.h>
#include <string.h>

static const char* TAG = "mcp_bridge";
static bool s_initialized = false;

int mcp_bridge_init(void)
{
    if (s_initialized) {
        return OK;
    }

    int ret = mcp_tool_registry_init();
    if (ret < 0) {
        syslog(LOG_ERR, "[%s] mcp_tool_registry_init failed: %d\n", TAG, ret);
        return ERROR;
    }

    ret = mcp_register_builtin_tools();
    if (ret < 0) {
        syslog(LOG_ERR, "[%s] mcp_register_builtin_tools failed: %d\n", TAG, ret);
        mcp_tool_registry_cleanup();
        return ERROR;
    }

    s_initialized = true;
    syslog(LOG_INFO, "[%s] MCP bridge initialized\n", TAG);
    return OK;
}

int mcp_bridge_execute(const char* name, const char* input_json, char* output,
    size_t output_size)
{
    if (!s_initialized || !name || !output || output_size == 0) {
        return ERROR;
    }

    /* mcp_tool_registry_execute_tool returns a JSON-RPC response (heap).
     * We need to extract the actual result content from it. */
    char* rpc_response = mcp_tool_registry_execute_tool(name, input_json ? input_json : "{}");
    if (!rpc_response) {
        return ERROR;
    }

    /* Parse JSON-RPC response to extract result.content[0].text */
    cJSON* root = cJSON_Parse(rpc_response);
    free(rpc_response);

    if (!root) {
        snprintf(output, output_size, "{\"error\":\"MCP parse error\"}");
        return ERROR;
    }

    /* Check for JSON-RPC error */
    cJSON* error = cJSON_GetObjectItem(root, "error");
    if (error) {
        cJSON* msg = cJSON_GetObjectItem(error, "message");
        snprintf(output, output_size, "{\"error\":\"%s\"}",
            (msg && cJSON_IsString(msg)) ? msg->valuestring : "MCP error");
        cJSON_Delete(root);
        return ERROR;
    }

    /* Extract result → content[0] → text */
    cJSON* result = cJSON_GetObjectItem(root, "result");
    if (result) {
        cJSON* content = cJSON_GetObjectItem(result, "content");
        if (content && cJSON_IsArray(content)) {
            cJSON* first = cJSON_GetArrayItem(content, 0);
            if (first) {
                cJSON* text = cJSON_GetObjectItem(first, "text");
                if (text && cJSON_IsString(text)) {
                    strncpy(output, text->valuestring, output_size - 1);
                    output[output_size - 1] = '\0';
                    cJSON_Delete(root);
                    return OK;
                }
            }
        }
        /* Fallback: print the whole result object */
        char* result_str = cJSON_PrintUnformatted(result);
        if (result_str) {
            strncpy(output, result_str, output_size - 1);
            output[output_size - 1] = '\0';
            free(result_str);
        }
    }

    cJSON_Delete(root);
    return OK;
}

char* mcp_bridge_get_tools_json(void)
{
    if (!s_initialized) {
        return NULL;
    }

    /* Get JSON-RPC tools/list response from MCP registry */
    char* rpc_response = mcp_tool_registry_get_tools_json();
    if (!rpc_response) {
        return NULL;
    }

    /* Parse: {"jsonrpc":"2.0","result":{"tools":[...]}} */
    cJSON* root = cJSON_Parse(rpc_response);
    free(rpc_response);
    if (!root) {
        return NULL;
    }

    cJSON* result = cJSON_GetObjectItem(root, "result");
    cJSON* tools = result ? cJSON_GetObjectItem(result, "tools") : NULL;
    if (!tools || !cJSON_IsArray(tools)) {
        cJSON_Delete(root);
        return NULL;
    }

    /* Convert MCP tool format to AI Agent format:
     * MCP:  {"name":"...", "description":"...", "inputSchema":{...}}
     * Vela: {"name":"...", "description":"...", "input_schema":{...}}
     */
    cJSON* vela_arr = cJSON_CreateArray();
    cJSON* mcp_tool = NULL;
    cJSON_ArrayForEach(mcp_tool, tools)
    {
        cJSON* name = cJSON_GetObjectItem(mcp_tool, "name");
        cJSON* desc = cJSON_GetObjectItem(mcp_tool, "description");
        cJSON* schema = cJSON_GetObjectItem(mcp_tool, "inputSchema");

        if (!name || !cJSON_IsString(name))
            continue;

        cJSON* vt = cJSON_CreateObject();
        cJSON_AddStringToObject(vt, "name", name->valuestring);
        cJSON_AddStringToObject(vt, "description",
            (desc && cJSON_IsString(desc)) ? desc->valuestring
                                           : "");
        if (schema) {
            cJSON_AddItemToObject(vt, "input_schema", cJSON_Duplicate(schema, 1));
        } else {
            /* Ensure every tool has a valid input_schema */
            cJSON* empty = cJSON_CreateObject();
            cJSON_AddStringToObject(empty, "type", "object");
            cJSON_AddItemToObject(empty, "properties", cJSON_CreateObject());
            cJSON_AddItemToObject(vt, "input_schema", empty);
        }
        cJSON_AddItemToArray(vela_arr, vt);
    }

    char* json = cJSON_PrintUnformatted(vela_arr);
    cJSON_Delete(vela_arr);
    cJSON_Delete(root);
    return json;
}

void mcp_bridge_cleanup(void)
{
    if (s_initialized) {
        mcp_tool_registry_cleanup();
        s_initialized = false;
        syslog(LOG_INFO, "[%s] MCP bridge cleaned up\n", TAG);
    }
}
