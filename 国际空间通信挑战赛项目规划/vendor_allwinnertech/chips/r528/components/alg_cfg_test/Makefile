include $(APPDIR)/Make.defs

PROGNAME = test_alg_cfg
PRIORITY = 100
STACKSIZE = 4096
MODULE = $(CONFIG_AW_SUNXI_ALG_CFG_TEST)

MAINSRC = test_alg_cfg.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/internal_inc
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/hal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/osal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source/sound/common
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/../nuttx/arch/arm/src/armv7-a

include $(APPDIR)/Application.mk

