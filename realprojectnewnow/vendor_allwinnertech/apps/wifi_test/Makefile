############################################################################
# apps/showlogo/Makefile
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

# TEST WIFI CMD built-in application info

PRIORITY  = $(CONFIG_WIFI_TEST_PRIORITY)
STACKSIZE = $(CONFIG_WIFI_TEST_STACKSIZE)
MODULE    = $(CONFIG_WIFI_TEST)

# TEST WIFI CMD
PROGNAME += test_wifi
MAINSRC += test_sdio_wifi.c

# Use common WiFi includes
$(eval $(AW_ADD_WIFI_INCLUDES))

include $(APPDIR)/Application.mk
