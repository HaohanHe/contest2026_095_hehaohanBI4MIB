############################################################################
# Contest 2026 team 095 AI Radio Console - Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = ai_radio
PRIORITY  = 100
STACKSIZE = 16384
MODULE    = $(CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE)

CSRCS = src/agent_bridge.c \
        src/audio_capture.c \
        src/cw_decoder.c \
        src/freq_recommender.c \
        src/mayday_detector.c \
        src/radio_log.c \
        src/signal_analyzer.c \
        src/translator.c

MAINSRC = src/main.c

CFLAGS += -I$(CURDIR)/include

include $(APPDIR)/Application.mk
