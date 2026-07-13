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
 * heartbeat.h — Periodic task checker for AI Agent (Vela/NuttX port)
 *
 * Uses POSIX-compatible types via agent_compat.h.
 */

#include "agent_compat.h"
#include <stdbool.h>

/**
 * Initialize the heartbeat service (logs ready state).
 */
int heartbeat_init(void);

/**
 * Start the heartbeat timer thread. Checks HEARTBEAT.md periodically
 * and sends a prompt to the agent if actionable tasks are found.
 */
int heartbeat_start(void);

/**
 * Stop the heartbeat timer thread.
 */
void heartbeat_stop(void);

/**
 * Manually trigger a heartbeat check (for CLI testing).
 * Returns true if the agent was prompted, false if no tasks found.
 */
bool heartbeat_trigger(void);
