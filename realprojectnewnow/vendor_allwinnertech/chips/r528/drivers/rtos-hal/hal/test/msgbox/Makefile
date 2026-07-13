include $(APPDIR)/Make.defs

PROGNAME = test_msgbox
PRIORITY = SCHED_PRIORITY_DEFAULT
STACKSIZE = $(CONFIG_DEFAULT_TASK_STACKSIZE)
MODULE = $(CONFIG_HAL_TEST_MSGBOX)

MAINSRC = test_msgbox.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/hal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/osal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/../nuttx/arch/arm/src/armv7-a

include $(APPDIR)/Application.mk

