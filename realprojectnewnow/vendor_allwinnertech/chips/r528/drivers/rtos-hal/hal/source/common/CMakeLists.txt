if(CONFIG_DRIVERS_DMA)
    list(APPEND SRCS dma_alloc.c)
endif()

target_sources(arch PRIVATE ${SRCS})
