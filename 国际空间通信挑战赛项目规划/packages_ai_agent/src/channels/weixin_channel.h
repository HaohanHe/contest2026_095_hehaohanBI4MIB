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

#ifdef CONFIG_AI_AGENT_WEIXIN

int weixin_channel_init(void);
int weixin_channel_start(void);
int weixin_channel_send(const char* to_user_id,
    const char* context_token,
    const char* text);
void weixin_channel_stop(void);
int weixin_channel_set_token(const char* token, unsigned int uin);
int weixin_channel_login(char* qr_url, size_t qr_cap,
    char* qrcode_id, size_t qrc_cap);
int weixin_channel_poll_login(const char* qrcode_id);

#else /* stubs */

static inline int weixin_channel_init(void) { return 0; }
static inline int weixin_channel_start(void) { return 0; }
static inline int weixin_channel_send(const char* u, const char* c, const char* t) { (void)u; (void)c; (void)t; return -1; }
static inline void weixin_channel_stop(void) { }
static inline int weixin_channel_set_token(const char* t, unsigned int u) { (void)t; (void)u; return -1; }
static inline int weixin_channel_login(char* q, size_t qc, char* i, size_t ic) { (void)q; (void)qc; (void)i; (void)ic; return -1; }
static inline int weixin_channel_poll_login(const char* q) { (void)q; return -1; }

#endif

#ifdef __cplusplus
}
#endif
