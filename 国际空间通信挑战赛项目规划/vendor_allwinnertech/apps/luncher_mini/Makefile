############################################################################
# apps/examples/helloxx/Makefile
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

MAINSRC = luncher_mini.c
CSRCS = lv_demo_panel_rgb_control.c
# mcpserver built-in application info

PROGNAME = $(CONFIG_LUNCHER_MINI_APP_PROGNAME)
PRIORITY = $(CONFIG_LUNCHER_MINI_APP_PRIORITY)
STACKSIZE = $(CONFIG_LUNCHER_MINI_APP_STACKSIZE)
MODULE = $(CONFIG_LUNCHER_MINI_APP)

# Use common HAL includes
$(eval $(AW_ADD_HAL_INCLUDES))

# Define LED configuration for WS2812
CFLAGS += -DCONFIG_LED_RGB_WS2812

include $(APPDIR)/Application.mk

 