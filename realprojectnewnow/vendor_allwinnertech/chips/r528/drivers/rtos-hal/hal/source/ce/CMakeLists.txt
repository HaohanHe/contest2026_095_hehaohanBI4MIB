if(CONFIG_DRIVERS_CE)
    list(APPEND SRCS hal_ce.c ce_common.c dev_rng.c)
endif()

target_sources(arch PRIVATE ${SRCS})
