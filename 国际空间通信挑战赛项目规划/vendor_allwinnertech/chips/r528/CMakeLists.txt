# ##############################################################################
# vendor/allwinnertech/chips/r528/CMakeLists.txt
#
# 基于Make.defs转换的CMake构建脚本
#
# ##############################################################################

message(STATUS "PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
#message(STATUS "CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR}")

if(CONFIG_ARCH_SUN8IW20)
	message(STATUS "ARCH: CONFIG_ARCH_SUN8IW20")
endif()

#target_include_directories(arch PRIVATE ${NUTTX_CHIP_ABS_DIR}/include)

target_include_directories(
	arch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/internal_inc
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware
    ${CMAKE_CURRENT_SOURCE_DIR}/../..//xiaomi/mitee_iot/ca/triad
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/rtos-hal/include/osal
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/rtos-hal/include/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/rtos-hal/hal/source
    ${CMAKE_CURRENT_SOURCE_DIR}/components/secure_storage
    ${PROJECT_SOURCE_DIR}/../vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source/hwspinlock
    ${PROJECT_SOURCE_DIR}/../vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source/timer
    ${PROJECT_SOURCE_DIR}/../frameworks/security/include
    ${PROJECT_SOURCE_DIR}/../nuttx/arch/arm/include/armv7-a/
)

#add_compile_options(-include ${CMAKE_BINARY_DIR}/include/nuttx/config.h)

# 定义芯片级源文件列表
set(CHIP_CSRCS
    r528_boot.c
    r528_serial.c
    r528_irq.c
    spare_head.c
    r528_lowputc.c
    secondary.c
    mi_boardctl.c
    mi_hw_version.c
)

# 条件编译处理
if(CONFIG_ARCH_IDLE_CUSTOM)
    list(APPEND CHIP_CSRCS r528_idle.c)
endif()

if(CONFIG_BOARD_USBDEV_SERIALSTR)
    list(APPEND CHIP_CSRCS mi_serialstr.c)
endif()

if(CONFIG_SMP)
    list(APPEND CHIP_CSRCS r528_cpuboot.c)
endif()

if(CONFIG_SCHED_TICKLESS)
	if(NOT CONFIG_ONESHOT_AS_TICK)
    	list(APPEND CHIP_CSRCS r528_tickless_timer.c)
	endif()
else()
    list(APPEND CHIP_CSRCS r528_timerisr.c)
endif()

if(CONFIG_R528_WATCHDOG)
    list(APPEND CHIP_CSRCS r528_wdt.c sunxi_hal_wdt.c)
endif()

if(CONFIG_R528_RESET)
    list(APPEND CHIP_CSRCS r528_reset.c)
endif()

if(CONFIG_AW_RPTUN)
    list(APPEND CHIP_CSRCS dsp_boot.c r528_rptun.c r528_dsp_wdt.c)
endif()

if(CONFIG_AW_RPTUN_SECURE)
    list(APPEND CHIP_CSRCS r528_rptun_secure.c)
endif()

if(CONFIG_IEEE80211_REALTEK_WIFI)
    list(APPEND CHIP_CSRCS r528_wlan.c)
endif()

if(CONFIG_R528_PROTECT_BROM)
    list(APPEND CHIP_CSRCS r528_watchpoint.c)
endif()

target_sources(arch PRIVATE ${CHIP_CSRCS})

if(CONFIG_ARCH_TRUSTZONE_SECURE)
	nuttx_add_extra_library(${CMAKE_CURRENT_SOURCE_DIR}/libr528_secure.a)
endif()

# 包含子目录构建
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/drivers)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/components)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/drv)

# 生成目标（示例，根据实际项目类型调整）
#add_library(aw_r528_chip STATIC ${CHIP_CSRCS})
#target_link_libraries(aw_r528_chip ${EXTRA_LIBS})
