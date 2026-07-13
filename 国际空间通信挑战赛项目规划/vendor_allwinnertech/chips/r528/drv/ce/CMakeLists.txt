if(CONFIG_DRIVERS_CE)
list(APPEND SRCS
	drv_ce.c
	hw_hash.c
	hw_aes.c
)
target_sources(arch PRIVATE ${SRCS})
endif()
