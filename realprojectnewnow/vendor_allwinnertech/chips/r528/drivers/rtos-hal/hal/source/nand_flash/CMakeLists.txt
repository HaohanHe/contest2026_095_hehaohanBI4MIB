if(CONFIG_DRIVERS_NAND_FLASH)
	if(CONFIG_MM_KASAN)
		nuttx_add_extra_library(${CMAKE_CURRENT_SOURCE_DIR}/libnuttx_sun8iw20p1_nandflash_kasan.a)
	else()
		nuttx_add_extra_library(${CMAKE_CURRENT_SOURCE_DIR}/libnuttx_sun8iw20p1_nandflash.a)
	endif()

    target_include_directories(
        arch PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

 	list(APPEND SRCS nuttx_nand_init.c nuttx_nand_debug.c req_proxy.c src/nand_osal_for_rtos.c src/nand_format.c src/nand_phy.c src/nand_scan.c src/nand_global.c src/uboot_tail_spinand.c src/nand_id.c src/nand_physic_interface_spinand.c src/spic_op.c src/nand_ecc_op.c src/spi_nand2.c src/build_nand_partition.c src/build_phy_partition.c src/nand_info_init.c src/nand_physic_interface.c src/nand_secure_storage.c src/nand_ota.c src/uboot_tail.c)
endif()
target_sources(arch PRIVATE ${SRCS})
