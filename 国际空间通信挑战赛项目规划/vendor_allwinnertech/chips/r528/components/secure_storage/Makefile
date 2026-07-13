include $(APPDIR)/Make.defs

PROGNAME = mikey
PRIORITY = 100
STACKSIZE = 8192
MODULE = $(CONFIG_AW_SUNXI_SECURE_STORAGE_TEST)

MAINSRC = secure_storage_test.c
CSRCS += sunxi_secure_storage.c
CSRCS += sunxi_secure_storage_warpper.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/internal_inc
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/hal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/osal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/xiaomi/mitee_iot/ca/triad
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/../nuttx/arch/arm/src/armv7-a
#EXTRA


clean::
	rm -f *.o .built

distclean::
	rm -f .depend Make.deps *.o .built

include $(APPDIR)/Application.mk

