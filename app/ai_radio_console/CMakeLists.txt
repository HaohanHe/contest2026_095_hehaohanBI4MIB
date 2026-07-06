# Contest 2026 team 095 AI Radio Console
if(CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE)
  file(GLOB CSRCS src/*.c)
  list(FILTER CSRCS EXCLUDE REGEX "main\\.c$")
  nuttx_add_application(
    NAME ai_radio
    SRCS ${CSRCS} src/main.c
    INCLUDE_DIRECTORIES include
    STACKSIZE 16384
    PRIORITY 100)
endif()
