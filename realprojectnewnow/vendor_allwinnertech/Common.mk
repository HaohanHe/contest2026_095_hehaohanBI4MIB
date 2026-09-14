############################################################################
# vendor/allwinnertech/Common.mk
#
# Common definitions for Allwinnertech vendor build.
# Include this file in apps or other modules that need access to
# Allwinnertech headers.
#
############################################################################

ifndef AW_COMMON_INCLUDED
AW_COMMON_INCLUDED = 1

# Base paths
AW_VENDOR_DIR ?= $(TOPDIR)/../vendor/allwinnertech
AW_CHIP_DIR   ?= $(AW_VENDOR_DIR)/chips/r528
AW_BOARD_DIR  ?= $(AW_VENDOR_DIR)/boards/r528

# RTOS-HAL paths
AW_HAL_DIR    ?= $(AW_CHIP_DIR)/drivers/rtos-hal
AW_HAL_INCDIR  = $(AW_HAL_DIR)/include/hal
AW_OSAL_INCDIR = $(AW_HAL_DIR)/include/osal
AW_HAL_SRCDIR  = $(AW_HAL_DIR)/hal/source

# Realtek WiFi driver paths (if applicable)
AW_WIFI_DIR    = $(AW_BOARD_DIR)/drivers/realtek_ieee80211

# Helper function to add all common HAL include paths
define AW_ADD_HAL_INCLUDES
CFLAGS += ${INCDIR_PREFIX}$(AW_HAL_INCDIR)
CFLAGS += ${INCDIR_PREFIX}$(AW_HAL_INCDIR)/sdmmc
CFLAGS += ${INCDIR_PREFIX}$(AW_OSAL_INCDIR)
CFLAGS += ${INCDIR_PREFIX}$(AW_HAL_SRCDIR)
CFLAGS += ${INCDIR_PREFIX}$(AW_HAL_SRCDIR)/common
CFLAGS += ${INCDIR_PREFIX}$(AW_HAL_SRCDIR)/gpio
CFLAGS += ${INCDIR_PREFIX}$(AW_HAL_SRCDIR)/hwspinlock
CFLAGS += ${INCDIR_PREFIX}$(AW_HAL_SRCDIR)/timer
endef

# Helper function to add Realtek WiFi includes
define AW_ADD_WIFI_INCLUDES
CFLAGS += ${INCDIR_PREFIX}$(AW_WIFI_DIR)
CFLAGS += ${INCDIR_PREFIX}$(AW_WIFI_DIR)/api/wifi
CFLAGS += ${INCDIR_PREFIX}$(AW_WIFI_DIR)/include
CFLAGS += ${INCDIR_PREFIX}$(AW_WIFI_DIR)/os/customer_rtos
CFLAGS += ${INCDIR_PREFIX}$(AW_WIFI_DIR)/os/os_dep
CFLAGS += ${INCDIR_PREFIX}$(AW_WIFI_DIR)/os/os_dep/include
CFLAGS += ${INCDIR_PREFIX}$(AW_WIFI_DIR)/platform/include
CFLAGS += ${INCDIR_PREFIX}$(AW_WIFI_DIR)/platform/rtwlan_bsp
CFLAGS += ${INCDIR_PREFIX}$(AW_WIFI_DIR)/platform/sdio
CFLAGS += ${INCDIR_PREFIX}$(AW_WIFI_DIR)/platform/sdio/core
CFLAGS += ${INCDIR_PREFIX}$(AW_WIFI_DIR)/platform/sdio/include
endef

endif # AW_COMMON_INCLUDED
