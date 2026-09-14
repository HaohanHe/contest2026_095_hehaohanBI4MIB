############################################################################
# Contest 2026 team 095 DayNote - Daily Voice Notes
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = daynote
PRIORITY  = 100
STACKSIZE = 49152

MODULE    = $(CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE)

# Source files live under src/
VPATH += $(CURDIR)/src

CSRCS += agent_bridge.c \
          asr_engine.c audio_capture.c audio_i2s.c \
          ai_radio_font.c \
          config_store.c \
          daynote_agent.c \
          input_lradc.c json_minimal.c llm_analyzer.c \
          siliconflow_client.c spacelog_settings.c \
          ui_daynote.c \
          wav_encoder.c wifi_auto_connect.c \
          vad_detector.c auto_recorder.c \
          note_store.c memory_index.c \
          daily_digest.c daily_summary.c translator.c

MAINSRC = main.c

CFLAGS += -I$(CURDIR)/include
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/graphics/lvgl
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/graphics/lvgl/lvgl
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/../frameworks/system/topics/include
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/../frameworks/system/utils/include
LDLIBS += -lm

include $(APPDIR)/Application.mk
