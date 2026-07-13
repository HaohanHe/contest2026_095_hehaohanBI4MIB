if(CONFIG_DRIVERS_EFUSE)
    list(APPEND SRCS efuse.c hal_efuse.c)
endif()
target_sources(arch PRIVATE ${SRCS})
