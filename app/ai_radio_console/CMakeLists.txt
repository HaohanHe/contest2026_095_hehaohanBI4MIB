# Contest 2026 team 095 DayNote - Daily Voice Notes
if(CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE)
  file(GLOB CSRCS src/*.c)
  list(FILTER CSRCS EXCLUDE REGEX "src/main\\.c$")
  nuttx_add_application(
    NAME daynote
    SRCS ${CSRCS} src/main.c
    INCLUDE_DIRECTORIES include
    STACKSIZE 49152
    PRIORITY 100)
  target_include_directories(daynote PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../lvgl
    ${CMAKE_CURRENT_LIST_DIR}/../lvgl/lvgl)
endif()
