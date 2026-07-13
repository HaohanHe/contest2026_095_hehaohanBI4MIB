if(CONFIG_DRIVERS_DMA)
    list(APPEND SRCS hal_dma.c)
endif()
target_sources(arch PRIVATE ${SRCS})
