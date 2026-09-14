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

#include "tools/mcp_tool_registry.h"
#include "tools/mcp_server.h"
#include "cJSON.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <syslog.h>

static const char *TAG = "mcp_reg";

typedef struct {
    mcp_server_t *mcp_server;
    pthread_rwlock_t lock;
    bool initialized;
} tool_registry_t;

static tool_registry_t g_registry = {0};

int mcp_tool_registry_init(void)
{
    if (g_registry.initialized) return 0;

    g_registry.mcp_server = malloc(sizeof(mcp_server_t));
    if (!g_registry.mcp_server) return -1;

    int ret = mcp_server_init(g_registry.mcp_server, "agent-mcp", "1.0.0");
    if (ret < 0) {
        free(g_registry.mcp_server);
        g_registry.mcp_server = NULL;
        return ret;
    }

    pthread_rwlock_init(&g_registry.lock, NULL);
    g_registry.initialized = true;
    syslog(LOG_INFO, "[%s] MCP tool registry initialized\n", TAG);
    return 0;
}

int mcp_tool_registry_register(const char *name, const char *description,
                                const mcp_param_t *params, size_t param_count,
                                mcp_tool_handler_t handler, void *user_data)
{
    if (!g_registry.initialized || !name || !description || !handler) return -1;

    pthread_rwlock_wrlock(&g_registry.lock);
    int ret = mcp_server_register_tool(g_registry.mcp_server, name, description,
                                        params, (int)param_count,
                                        (mcp_tool_fn)handler, user_data, false);
    pthread_rwlock_unlock(&g_registry.lock);
    if (ret == 0) syslog(LOG_INFO, "[%s] Registered MCP tool: %s\n", TAG, name);
    return ret;
}

char *mcp_tool_registry_get_tools_json(void)
{
    if (!g_registry.initialized) return NULL;

    pthread_rwlock_rdlock(&g_registry.lock);

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "jsonrpc", "2.0");
    cJSON_AddStringToObject(req, "method", "tools/list");
    cJSON_AddStringToObject(req, "id", "1");

    char *req_json = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_json) { pthread_rwlock_unlock(&g_registry.lock); return NULL; }

    char *resp = mcp_server_handle_request(g_registry.mcp_server, req_json);
    free(req_json);
    pthread_rwlock_unlock(&g_registry.lock);
    return resp;
}

char *mcp_tool_registry_execute_tool(const char *name, const char *args_json)
{
    if (!g_registry.initialized || !name || !args_json) return NULL;

    pthread_rwlock_rdlock(&g_registry.lock);

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "jsonrpc", "2.0");
    cJSON_AddStringToObject(req, "method", "tools/call");
    cJSON_AddStringToObject(req, "id", "1");

    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "name", name);
    cJSON *arguments = cJSON_Parse(args_json);
    cJSON_AddItemToObject(params, "arguments", arguments ? arguments : cJSON_CreateObject());
    cJSON_AddItemToObject(req, "params", params);

    char *req_json = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_json) { pthread_rwlock_unlock(&g_registry.lock); return NULL; }

    char *resp = mcp_server_handle_request(g_registry.mcp_server, req_json);
    free(req_json);
    pthread_rwlock_unlock(&g_registry.lock);
    return resp;
}

void mcp_tool_registry_cleanup(void)
{
    if (!g_registry.initialized) return;
    pthread_rwlock_wrlock(&g_registry.lock);
    if (g_registry.mcp_server) {
        mcp_server_destroy(g_registry.mcp_server);
        free(g_registry.mcp_server);
        g_registry.mcp_server = NULL;
    }
    g_registry.initialized = false;
    pthread_rwlock_unlock(&g_registry.lock);
    pthread_rwlock_destroy(&g_registry.lock);
}
