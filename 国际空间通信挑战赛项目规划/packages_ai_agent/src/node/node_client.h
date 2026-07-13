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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the node client (does not connect yet).
 * @return OK on success
 */
int node_client_init(void);

/**
 * Start the node client — connects to the configured Gateway.
 * Requires gateway_host and gateway_token to be set via config_store.
 * @return OK on success, ERROR if config missing or connect fails
 */
int node_client_start(void);

/**
 * Stop the node client and disconnect from Gateway.
 */
void node_client_stop(void);

/**
 * Forward a chat message to the gateway over the node WebSocket connection.
 * Used when the local node lacks feishu credentials and needs the gateway
 * to deliver the message on its behalf.
 *
 * @param channel  Channel identifier, e.g. "feishu"
 * @param chat_id  Target chat ID
 * @param content  Message text
 * @return OK on success, ERROR if not connected or send fails
 */
int node_client_send_chat_message(const char* channel, const char* chat_id,
    const char* content);

#ifdef __cplusplus
}
#endif
