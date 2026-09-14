target_include_directories(
	arch PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/rtos-hal/include
	${CMAKE_CURRENT_SOURCE_DIR}/rtos-hal/include/hal
	${CMAKE_CURRENT_SOURCE_DIR}/rtos-hal/include/hal/sdmmc
	${CMAKE_CURRENT_SOURCE_DIR}/rtos-hal/include/hal/sdmmc/hal
	${CMAKE_CURRENT_SOURCE_DIR}/rtos-hal/include/hal/sdmmc/osal
)

nuttx_add_subdirectory()
