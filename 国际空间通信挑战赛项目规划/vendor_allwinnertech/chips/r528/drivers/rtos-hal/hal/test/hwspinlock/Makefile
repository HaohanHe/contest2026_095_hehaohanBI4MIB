include $(APPDIR)/Make.defs

PROGNAME = test_hwspinlock_get test_hwspinlock_put
PRIORITY = 100
STACKSIZE = 4096

MODULE = $(CONFIG_HAL_TEST_HWSPINLOCK)

MAINSRC = test_hwspinlock_get.c test_hwspinlock_put.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/hal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/osal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/../nuttx/arch/arm/src/armv7-a

include $(APPDIR)/Application.mk
