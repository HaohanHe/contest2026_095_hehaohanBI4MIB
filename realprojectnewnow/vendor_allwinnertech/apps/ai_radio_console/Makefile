############################################################################
# apps/vendor/allwinnertech/apps/ai_radio_console/Makefile
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

include $(APPDIR)/Make.defs
include $(APPDIR)/vendor/allwinnertech/Common.mk

CSRCS = radio_ui.c audio_recorder.c ptt_control.c asr_client.c radio_agent.c

ifeq ($(CONFIG_AI_RADIO_CONSOLE_SERVER),y)
CSRCS += radio_server.c
endif

# ai_agent headers (velaclaw/client.h, etc.)
# The package may be cloned as packages/ai_agent or packages_ai_agent depending
# on the manifest; accept either location so the build works in both layouts.
AI_AGENT_BASE_DIR := $(wildcard $(APPDIR)/packages/ai_agent)
ifeq ($(AI_AGENT_BASE_DIR),)
AI_AGENT_BASE_DIR := $(wildcard $(APPDIR)/packages_ai_agent)
endif
ifeq ($(AI_AGENT_BASE_DIR),)
AI_AGENT_BASE_DIR := $(wildcard $(APPDIR)/../../packages_ai_agent)
endif
ifeq ($(AI_AGENT_BASE_DIR),)
AI_AGENT_BASE_DIR := $(wildcard $(APPDIR)/../../packages/ai_agent)
endif

ifneq ($(AI_AGENT_BASE_DIR),)
CFLAGS += ${INCDIR_PREFIX}$(AI_AGENT_BASE_DIR)/include
CFLAGS += ${INCDIR_PREFIX}$(AI_AGENT_BASE_DIR)/src
endif

$(eval $(AW_ADD_HAL_INCLUDES))

MAINSRC = main.c

PROGNAME = ai_radio_console
PRIORITY = CONFIG_AI_RADIO_CONSOLE_PRIORITY
STACKSIZE = CONFIG_AI_RADIO_CONSOLE_STACKSIZE
MODULE = $(CONFIG_AI_RADIO_CONSOLE)

include $(APPDIR)/Application.mk
