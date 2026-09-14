include $(APPDIR)/Make.defs

PROGNAME = test_g2d_blend test_g2d test_g2d_scale_job test_g2d_lbc_rot
PRIORITY = 100
STACKSIZE = 4096

MODULE = $(CONFIG_HAL_TEST_G2D)

MAINSRC = test_g2d_blend.c test_g2d.c test_g2d_scale_job.c test_g2d_lbc_rot.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/hal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/osal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source/g2d_rcq
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/../nuttx/arch/arm/src/armv7-a

include $(APPDIR)/Application.mk

