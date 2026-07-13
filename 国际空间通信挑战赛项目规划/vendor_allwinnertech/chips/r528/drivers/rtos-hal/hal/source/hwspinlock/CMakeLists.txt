if(CONFIG_DRIVERS_HWSPINLOCK)
    list(APPEND SRCS hal_hwspinlock.c)
endif()
target_sources(arch PRIVATE ${SRCS})
