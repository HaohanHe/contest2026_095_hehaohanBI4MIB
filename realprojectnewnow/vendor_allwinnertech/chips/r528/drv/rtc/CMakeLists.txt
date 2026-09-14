if(CONFIG_DRIVERS_RTC)
list(APPEND SRCS
	drv_rtc.c
)
target_sources(arch PRIVATE ${SRCS})
endif()
