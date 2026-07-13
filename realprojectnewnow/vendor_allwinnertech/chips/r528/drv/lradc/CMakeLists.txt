if(CONFIG_DRIVERS_LRADC)
list(APPEND SRCS
	drv_lradc.c
)
target_sources(arch PRIVATE ${SRCS})
endif()
