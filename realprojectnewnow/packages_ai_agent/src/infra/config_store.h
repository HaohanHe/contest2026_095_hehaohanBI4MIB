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
 * config_store.h — File-based key-value config (file-based key-value store for NuttX/Vela)
 *
 * Stores all runtime config in AGENT_CONFIG_FILE as a flat JSON object.
 * Thread-safe: protected by an internal mutex.
 */

#include "agent_compat.h"
#include <stddef.h>

/** Initialise the config store; creates directories and file if absent. */
int config_store_init(void);

/**
 * Read a string value.
 * @return OK if found, ERROR if key absent.
 */
int claw_config_get(const char *key, char *buf, size_t buf_size);

/** Write (or overwrite) a string value and persist to disk. */
int claw_config_set(const char *key, const char *value);

/** Delete a key. Returns OK even if key was absent. */
int config_del(const char *key);

/** Erase all stored keys. */
int config_erase_all(void);

/* Aliases used by network_manager and other modules */
#define agent_config_get claw_config_get
#define agent_config_set claw_config_set
