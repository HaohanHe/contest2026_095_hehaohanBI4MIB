include $(APPDIR)/Make.defs

PROGNAME = soundcard
PRIORITY = 100
STACKSIZE = 4096

MODULE = $(CONFIG_COMPONENTS_AW_ALSA_UTILS)

MAINSRC = card.c

ifeq ($(CONFIG_COMPONENTS_AW_ALSA_UTILS_APLAY),y)
PROGNAME += aplay
MAINSRC += aplay.c
endif

ifeq ($(CONFIG_COMPONENTS_AW_ALSA_UTILS_ARECORD),y)
PROGNAME += arecord
MAINSRC += arecord.c
endif


ifeq ($(CONFIG_COMPONENTS_AW_ALSA_UTILS_AMIXER),y)
PROGNAME += amixer
MAINSRC += amixer.c
endif

ifeq ($(CONFIG_COMPONENTS_AW_ALSA_UTILS_ALOOP),y)
PROGNAME += aloop
MAINSRC += aloop.c
endif

CSRCS = common.c wav_parser.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/hal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/include/osal
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/../nuttx/arch/arm/src/armv7-a

clean::
	rm -f *.o .built

distclean:: clean
	rm -f .depend Make.deps

include $(APPDIR)/Application.mk
