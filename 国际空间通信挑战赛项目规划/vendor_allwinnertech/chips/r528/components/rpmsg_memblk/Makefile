include $(APPDIR)/Make.defs

PROGNAME = test_rpmsg_memblk
PRIORITY = SCHED_PRIORITY_DEFAULT
STACKSIZE = $(CONFIG_DEFAULT_TASK_STACKSIZE)
MODULE = $(CONFIG_AW_RPMSG_MEMBLK)

MAINSRC = test_rpmsg_memblk.c
CSRCS  += md5.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/hal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/osal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/../nuttx/arch/arm/src/armv7-a
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/components/rpmsg_memblk/include

clean::
	rm -f *.o .built

distclean::
	rm -f .depend Make.deps *.o .built

include $(APPDIR)/Application.mk

