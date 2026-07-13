#target_include_directories(
#	arch PRIVATE
#	${CMAKE_CURRENT_SOURCE_DIR}
#)

#if(CONFIG_DRIVERS_GPADC)
    list(APPEND SRCS sunxi_secure_storage.c sunxi_secure_storage_warpper.c)
#endif()
target_sources(arch PRIVATE ${SRCS})
#nuttx_add_kernel_library(g2d ${SRCS})
