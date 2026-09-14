if(CONFIG_DRIVERS_GPIO)
    list(APPEND SRCS hal_gpio.c sun8iw20/gpio-sun8iw20.c)
endif()
target_sources(arch PRIVATE ${SRCS})
