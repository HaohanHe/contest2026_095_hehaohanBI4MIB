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

/*
 * This file contains code derived from MimiClaw (https://github.com/memovai/mimiclaw)
 * Copyright (c) 2026 Ziboyan Wang, licensed under the MIT License.
 * See NOTICE file for the original MIT License terms.
 */

#pragma once

/**
 * message_bus_tap.h — outbound message tap mechanism
 *
 * Allows test code (or other subsystems) to intercept outbound messages
 * by channel name before they reach the normal dispatch logic.
 */

#include "core/message_bus.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Callback invoked when a tap matches an outbound message.
 *  The callback receives a pointer to the original message — it must
 *  copy any data it needs (msg->content etc.) before returning.
 *  The caller retains ownership of the message. */
typedef void (*mbus_tap_cb_t)(const agent_msg_t* msg, void* cookie);

/** Register a tap for a specific channel.  Only one tap per channel.
 *  Returns OK on success, ERROR if the channel is already tapped or
 *  all slots are full (max 4). */
int mbus_tap_register(const char* channel, mbus_tap_cb_t cb, void* cookie);

/** Unregister the tap for the given channel. */
void mbus_tap_unregister(const char* channel);

/** Called by outbound_dispatch_task before normal dispatch.
 *  Returns true if a tap consumed the message (caller should skip
 *  its own dispatch and free). */
bool mbus_tap_try_deliver(const agent_msg_t* msg);

#ifdef __cplusplus
}
#endif
