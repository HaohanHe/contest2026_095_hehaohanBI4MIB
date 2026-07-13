if(CONFIG_DRIVERS_SPI)
list(APPEND SRCS
	drv_spi.c
)
target_sources(arch PRIVATE ${SRCS})
endif()
