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

#include <nuttx/config.h>

void cmd_set_feishu_app(int argc, char** argv);
void cmd_set_feishu_user_token(int argc, char** argv);
void cmd_set_mqtt(int argc, char** argv);
#ifdef CONFIG_AI_AGENT_NODE
void cmd_node_list(void);
void cmd_node_start(void);
void cmd_node_stop(void);
#endif
void cmd_set_gateway(int argc, char** argv);
void cmd_set_weixin_token(int argc, char** argv);
void cmd_weixin_login(void);
