#target_include_directories(
#	arch PRIVATE
#	${CMAKE_CURRENT_SOURCE_DIR}
#)

if(CONFIG_DRIVERS_RTC)
    list(APPEND SRCS hal_rtc.c rtc-lib.c)
endif()
target_sources(arch PRIVATE ${SRCS})
#nuttx_add_kernel_library(g2d ${SRCS})
