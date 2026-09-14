include $(APPDIR)/Make.defs

PROGNAME = test_aw_eq
PRIORITY = 100
STACKSIZE = 8192

MODULE = $(CONFIG_COMPONENTS_AW_EQ_TEST)

MAINSRC = test_aw_eq.c

#EXTRA_LIBPATHS += -L $(APPDIR)/vendor/allwinnertech/chips/r528/components/aw_eq/
#EXTRA_LIBS += -laweq
#CFLAGS += -L $(APPDIR)/vendor/allwinnertech/chips/r528/components/aw_eq/
#CFLAGS += -laweq

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/components/aw_eq/include
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/../nuttx/arch/arm/src/armv7-a

clean::
	rm -f *.o .built

distclean::
	rm -f .depend Make.deps *.o .built

include $(APPDIR)/Application.mk


