############################################################################
# Contest 2026 team 095 AI Radio Console (DayNote) - Makefile
############################################################################

include $(APPDIR)/Make.defs

ifeq ($(CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE),y)
PROGNAME  = daynote
PRIORITY  = 100
STACKSIZE = 40960
MODULE    = $(CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE)

# VelaClaw / ai_agent framework headers, local dir, netutils and BLE stack
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/ai_agent/include
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/ai_agent/src
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/contest2026_095_ai_radio_console
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/include/netutils
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/frameworks/connectivity/bluetooth/framework/include

MAINSRC = daynote_main.c
CSRCS   = daynote_core.c daynote_ui.c daynote_mmdvm.c daynote_asr.c
endif

include $(APPDIR)/Application.mk
