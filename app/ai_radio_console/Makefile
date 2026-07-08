############################################################################
# Contest 2026 team 095 AI Radio Console - Makefile
############################################################################

include $(APPDIR)/Make.defs

VPATH += src

PROGNAME  = ai_radio
PRIORITY  = 100
STACKSIZE = 16384
MODULE    = $(CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE)

CSRCS += agent_bridge.c asr_engine.c audio_capture.c audio_i2s.c \
          config_store.c cw_decoder.c freq_recommender.c gps_receiver.c \
          input_lradc.c json_minimal.c llm_analyzer.c location_service.c \
          mayday_detector.c radio_log.c signal_analyzer.c siliconflow_client.c \
          translator.c ui_ai_radio.c wav_encoder.c wifi_auto_connect.c

MAINSRC = main.c

CFLAGS += -I$(CURDIR)/include
LDLIBS += -lm

include $(APPDIR)/Application.mk
