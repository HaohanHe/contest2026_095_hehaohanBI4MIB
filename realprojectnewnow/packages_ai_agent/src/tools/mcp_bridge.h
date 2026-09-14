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

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the MCP tool registry and register all builtin MCP tools.
 * Call this before tool_registry_init().
 *
 * @return OK on success, ERROR on failure
 */
int mcp_bridge_init(void);

/**
 * Try to execute a tool via the MCP registry.
 *
 * @param name        Tool name (e.g. "turn_light_on")
 * @param input_json  Arguments JSON string
 * @param output      Output buffer (caller-owned)
 * @param output_size Size of output buffer
 * @return OK if tool was found and executed, ERROR if not found or failed
 */
int mcp_bridge_execute(const char *name, const char *input_json,
                       char *output, size_t output_size);

/**
 * Get MCP tools as a cJSON array of tool objects matching AI Agent's format:
 *   [{"name":"...", "description":"...", "input_schema":{...}}, ...]
 *
 * Caller must free the returned string with free().
 * Returns NULL if no MCP tools are registered.
 */
char *mcp_bridge_get_tools_json(void);

/**
 * Cleanup MCP bridge resources.
 */
void mcp_bridge_cleanup(void);

#ifdef __cplusplus
}
#endif
