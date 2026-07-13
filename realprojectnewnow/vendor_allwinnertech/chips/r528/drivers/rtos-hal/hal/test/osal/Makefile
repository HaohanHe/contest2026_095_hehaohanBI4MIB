include $(APPDIR)/Make.defs

PROGNAME = CONFIG_OSAL_TEST_PROGNAME
PRIORITY = CONFIG_OSAL_TEST_PRIORITY
STACKSIZE = CONFIG_OSAL_TEST_STACKSIZE

MODULE = y

MAINSRC = test_osal.c

AW_SRCDIR = $(TOPDIR)/$(patsubst "%",%,$(CONFIG_ARCH_CHIP_CUSTOM_DIR))

CFLAGS += -I $(AW_SRCDIR)/drivers/rtos-hal/include/hal
CFLAGS += -I $(AW_SRCDIR)/drivers/rtos-hal/include/osal
CFLAGS += -I $(AW_SRCDIR)/drivers/rtos-hal/hal/source

include $(APPDIR)/Application.mk

