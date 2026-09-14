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

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NODE_MAX_NODES 8
#define NODE_MAX_COMMANDS 16
#define NODE_ID_LEN 32
#define NODE_CMD_LEN 64

/**
 * Initialize the node manager.
 */
int node_manager_init(void);

/**
 * Handle an incoming WebSocket message that might be a Node protocol frame.
 * Called from ws_server's client_thread when a message arrives.
 *
 * @param ws_fd   The WebSocket file descriptor
 * @param data    Raw JSON message
 * @param len     Message length
 * @return true if the message was handled as a Node protocol frame
 */
bool node_manager_handle_message(int ws_fd, const char* data, int len);

/**
 * Called when a WebSocket client disconnects.
 * Removes the node and unregisters its tools.
 */
void node_manager_on_disconnect(int ws_fd);

/**
 * Execute a remote node command (called from tool_registry fallthrough).
 *
 * @param tool_name  Full tool name like "node:watch-01:get_heartrate"
 * @param input_json Arguments JSON
 * @param output     Output buffer
 * @param output_size Buffer size
 * @return OK on success, ERROR on failure
 */
int node_manager_execute(const char* tool_name, const char* input_json,
    char* output, size_t output_size);

/**
 * Get all remote node tools as a JSON array string (AI Agent format).
 * Caller must free() the returned string.
 */
char* node_manager_get_tools_json(void);

/**
 * Get a summary of connected nodes for CLI display.
 */
int node_manager_list(char* buf, size_t size);

/**
 * Send connect.challenge to a newly connected WebSocket client.
 * Called from ws_server after handshake so Nodes can identify themselves.
 */
void node_manager_send_challenge(int ws_fd);

/**
 * Return the number of currently active (connected) nodes.
 */
int node_manager_active_count(void);

/**
 * Broadcast a chat message to all connected nodes via "chat.forward" event.
 * Used to relay messages that cannot be delivered via platform events
 * (e.g. bot-to-bot @mentions in Feishu).
 *
 * @param channel   Message channel (e.g. "feishu")
 * @param chat_id   Chat/group ID
 * @param content   Message text content
 * @return number of nodes the message was sent to
 */
int node_manager_broadcast_chat(const char* channel, const char* chat_id,
    const char* content);

#ifdef __cplusplus
}
#endif
