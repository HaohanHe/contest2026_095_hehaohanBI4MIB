############################################################################
# apps/factest/Makefile
#
#   Copyright (C) 2017 Gregory Nutt. All rights reserved.
#   Author: Gregory Nutt <gnutt@nuttx.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name NuttX nor the names of its contributors may be
#    used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
############################################################################

include $(APPDIR)/Make.defs
include $(APPDIR)/vendor/allwinnertech/Common.mk

#include $(APPDIR)/../nuttx/arch/arm/src/armv7-a/Make.defs

# DHCPC address renewal built-in application info

ifneq ($(CONFIG_BTN_RST_MON),)
PRIORITY = $(CONFIG_BTN_RST_MON_PRIORITY)
STACKSIZE = $(CONFIG_BTN_RST_MON_STACKSIZE)
MODULE = $(CONFIG_BTN_RST_MON)


PROGNAME += btn_rst_mon
MAINSRC += btn_rst_mon.c


CFLAGS   += -Wno-unused-function -Wno-unused-const-variable -Wno-unused-but-set-variable
CFLAGS   += -Wno-return-type -Wno-pointer-sign -Wno-format
CFLAGS   += -Wno-unused-variable -Wno-implicit-function-declaration -Wno-parentheses
CFLAGS   += -Wno-implicit-function-declaration -Wno-strict-prototypes -Wno-int-conversion
CFLAGS   += -Wno-implicit-int

depend::
	$(Q) touch $(MAINSRC)
endif

include $(APPDIR)/Application.mk
