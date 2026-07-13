if(CONFIG_DRIVERS_TWI)
list(APPEND SRCS
	drv_twi.c
)
target_sources(arch PRIVATE ${SRCS})
endif()
