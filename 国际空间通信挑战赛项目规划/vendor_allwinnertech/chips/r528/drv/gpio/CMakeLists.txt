if(CONFIG_DRIVERS_GPIO)
list(APPEND SRCS
	drv_gpio.c
)
target_sources(arch PRIVATE ${SRCS})
endif()
