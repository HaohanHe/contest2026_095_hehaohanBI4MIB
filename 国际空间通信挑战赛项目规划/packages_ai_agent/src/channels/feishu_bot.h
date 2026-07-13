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

#include "agent_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_AI_AGENT_FEISHU

int feishu_bot_init(void);
int feishu_bot_start(void);
int feishu_send_message(const char *chat_id, const char *text);
int feishu_set_app(const char *app_id, const char *app_secret);
int feishu_api_post(const char *path, const char *json_body,
                    char *resp_buf, size_t resp_cap);
int feishu_api_request(const char *method, const char *path,
                       const char *body, size_t body_len,
                       char *resp_buf, size_t resp_cap);
const char *feishu_get_app_id(void);
int feishu_api_post_as_user(const char *path, const char *json_body,
                            char *resp_buf, size_t resp_cap);
int feishu_set_user_token(const char *token);
int feishu_api_request_as_user(const char *method, const char *path,
                               const char *body, size_t body_len,
                               char *resp_buf, size_t resp_cap);

#else /* !CONFIG_AI_AGENT_FEISHU — stubs */

static inline int feishu_bot_init(void) { return 0; }
static inline int feishu_bot_start(void) { return 0; }
static inline int feishu_send_message(const char *c, const char *t) { (void)c; (void)t; return -1; }
static inline int feishu_set_app(const char *a, const char *s) { (void)a; (void)s; return -1; }
static inline int feishu_api_post(const char *p, const char *b, char *r, size_t c) { (void)p; (void)b; (void)r; (void)c; return -1; }
static inline int feishu_api_request(const char *m, const char *p, const char *b, size_t l, char *r, size_t c) { (void)m; (void)p; (void)b; (void)l; (void)r; (void)c; return -1; }
static inline const char *feishu_get_app_id(void) { return ""; }
static inline int feishu_api_post_as_user(const char *p, const char *b, char *r, size_t c) { (void)p; (void)b; (void)r; (void)c; return -1; }
static inline int feishu_set_user_token(const char *t) { (void)t; return -1; }
static inline int feishu_api_request_as_user(const char *m, const char *p, const char *b, size_t l, char *r, size_t c) { (void)m; (void)p; (void)b; (void)l; (void)r; (void)c; return -1; }

#endif /* CONFIG_AI_AGENT_FEISHU */

#ifdef __cplusplus
}
#endif
