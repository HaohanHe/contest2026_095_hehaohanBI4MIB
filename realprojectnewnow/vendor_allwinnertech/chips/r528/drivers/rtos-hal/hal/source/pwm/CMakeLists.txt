if(CONFIG_DRIVERS_PWM)
    list(APPEND SRCS hal_pwm.c)
endif()
target_sources(arch PRIVATE ${SRCS})
