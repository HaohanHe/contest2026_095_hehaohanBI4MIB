include $(APPDIR)/Make.defs

PROGNAME = test_ce
PRIORITY = 100
STACKSIZE = 4096

MODULE = $(CONFIG_HAL_TEST_CE)

MAINSRC = test_ce.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/hal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/osal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/../nuttx/arch/arm/src/armv7-a

include $(APPDIR)/Application.mk
