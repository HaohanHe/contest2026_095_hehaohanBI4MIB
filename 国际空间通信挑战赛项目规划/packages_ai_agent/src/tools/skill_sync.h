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
/**
 * skill_sync.h — Bitable skill sync for AI Agent
 *
 * Pull skills from a Feishu Bitable (multi-dimensional table) and
 * write them to the local skills directory. Supports incremental
 * (version-based) and full sync modes.
 */

#include "agent_config.h"

#if AGENT_SKILL_SYNC_ENABLED

/**
 * Sync skills from remote Feishu Bitable to local skills directory.
 * Blocks until complete or timeout. Safe to call from any thread.
 *
 * Prerequisites:
 *   - Network must be connected
 *   - Feishu app credentials must be configured
 *   - AGENT_SKILL_SYNC_APP_TOKEN and TABLE_ID must be set
 *
 * @return OK on success, ERROR on failure (config/network/API error).
 */
int skill_sync_from_bitable(void);

#endif /* AGENT_SKILL_SYNC_ENABLED */
