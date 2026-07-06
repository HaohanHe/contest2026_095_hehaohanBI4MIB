############################################################################
# Contest 2026 team 095 AI Radio Console - Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = ai_radio
PRIORITY  = 100
STACKSIZE = 16384
MODULE    = $(CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE)

CSRCS =

MAINSRC = src/main.c

CFLAGS += -I$(CURDIR)/include
LDLIBS += -lm

include $(APPDIR)/Application.mk
