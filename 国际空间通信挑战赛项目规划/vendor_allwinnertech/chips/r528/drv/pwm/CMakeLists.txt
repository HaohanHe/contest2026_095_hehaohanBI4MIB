if(CONFIG_DRIVERS_PWM)
list(APPEND SRCS
	drv_pwm.c
)
target_sources(arch PRIVATE ${SRCS})
endif()
