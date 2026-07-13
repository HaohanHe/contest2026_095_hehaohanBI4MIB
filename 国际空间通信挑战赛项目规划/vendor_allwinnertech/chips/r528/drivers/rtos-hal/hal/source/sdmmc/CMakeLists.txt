if(CONFIG_DRIVERS_SDMMC)
    list(APPEND SRCS core.c hal_sdpin.c hal_sdhost.c quirks.c test.c nuttx_sdio.c cmd/cmd_sd.c)
endif()
target_sources(arch PRIVATE ${SRCS})
