if(CONFIG_DRIVERS_TIMER)
list(APPEND SRCS
	drv_timer.c
	drv_timer_ng.c
)
target_sources(arch PRIVATE ${SRCS})
endif()
