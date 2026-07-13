include $(APPDIR)/Make.defs

PROGNAME = test_avs test_timer test_htimer
PRIORITY = 100
STACKSIZE = 4096

MODULE = $(CONFIG_HAL_TEST_TIMER)

MAINSRC = test_avs.c test_timer.c test_htimer.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/hal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/osal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/../nuttx/arch/arm/src/armv7-a/

include $(APPDIR)/Application.mk

