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

#ifdef CONFIG_AI_AGENT_MQTT

int mqtt_channel_init(void);
int mqtt_channel_start(void);
int mqtt_channel_send(const char *chat_id, const char *text);
void mqtt_channel_stop(void);

#else /* stubs */

static inline int mqtt_channel_init(void) { return 0; }
static inline int mqtt_channel_start(void) { return 0; }
static inline int mqtt_channel_send(const char *c, const char *t) { (void)c; (void)t; return -1; }
static inline void mqtt_channel_stop(void) { }

#endif

#ifdef __cplusplus
}
#endif
