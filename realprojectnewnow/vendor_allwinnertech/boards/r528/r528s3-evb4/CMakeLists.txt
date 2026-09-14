add_subdirectory(src)

#message(WARNING "NUTTX_BOARD_ABS_DIR: ${NUTTX_BOARD_ABS_DIR}")

set_property(GLOBAL PROPERTY LD_SCRIPT 
          ${NUTTX_BOARD_ABS_DIR}/scripts/sdram.ld.S)

add_custom_target(
  nuttx_post_build
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Generating: vela_${CONFIG_ARCH_BOARD_CUSTOM_NAME}.bin"
  COMMAND cp nuttx vela_${CONFIG_ARCH_BOARD_CUSTOM_NAME}.elf
  COMMAND cp nuttx.bin vela_${CONFIG_ARCH_BOARD_CUSTOM_NAME}.bin
  COMMAND cp nuttx.bin ${CMAKE_BINARY_DIR}/${CONFIG_ARCH_BOARD_CUSTOM_NAME}.fex
  COMMAND cp nuttx.map vela_${CONFIG_ARCH_BOARD_CUSTOM_NAME}.map)

file(GLOB EXTRA_LIBS
     ${NUTTX_BOARD_ABS_DIR}/libs/${CONFIG_ARCH_BOARD_CUSTOM_NAME}/*.a)
     
nuttx_add_extra_library(${EXTRA_LIBS})
