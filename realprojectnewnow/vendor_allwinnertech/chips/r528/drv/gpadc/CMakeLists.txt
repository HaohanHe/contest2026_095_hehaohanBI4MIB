if(CONFIG_DRIVERS_GPADC)
list(APPEND SRCS
	drv_gpadc.c
)
target_sources(arch PRIVATE ${SRCS})
endif()
