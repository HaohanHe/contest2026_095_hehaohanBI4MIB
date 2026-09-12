#
# Copyright (C) 2026 Xiaomi Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

include $(APPDIR)/Make.defs

ifeq ($(CONFIG_LVX_USE_DEMO_DAYNOTE), y)
PROGNAME  = daynote
PRIORITY  = 100
STACKSIZE = 40960
MODULE    = $(CONFIG_LVX_USE_DEMO_DAYNOTE)

# AI Agent client SDK include path + local headers (flat structure)
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/ai_agent/include
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/ai_agent/src
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_095_ai_radio_console
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/include/netutils
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/frameworks/connectivity/bluetooth/framework/include

CSRCS   = daynote_core.c daynote_ui.c daynote_mmdvm.c daynote_asr.c
MAINSRC = daynote_main.c
endif

include $(APPDIR)/Application.mk
