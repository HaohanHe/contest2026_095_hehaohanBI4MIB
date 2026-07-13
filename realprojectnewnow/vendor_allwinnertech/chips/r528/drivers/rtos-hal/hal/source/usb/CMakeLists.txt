# ##############################################################################
# chips/r528/drivers/rtos-hal/hal/source/usb/CMakeLists.txt
#
# 基于make.defs转换的USB模块CMake构建脚本
# ##############################################################################

# 主USB模块条件编译
if(CONFIG_DRIVERS_USB)
	target_include_directories(
		arch PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/common
		${CMAKE_CURRENT_SOURCE_DIR}/sun20iw1
	)
    # 公共源文件列表
    set(USB_COMMON_CSRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/common/list_head_ext.c
        ${CMAKE_CURRENT_SOURCE_DIR}/common/usb_phy.c
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/sun20iw1/usb_sun20iw1.c
    )

    # 添加到芯片级源文件列表
    list(APPEND USB_SRCS ${USB_COMMON_CSRCS})
endif()

# USB设备功能条件编译
if(CONFIG_USB_DEVICE)
	target_include_directories(
		arch PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/common
		${CMAKE_CURRENT_SOURCE_DIR}/udc
	)
    # udc源文件列表
    set(USB_UDC_CSRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/udc/udc.c
        ${CMAKE_CURRENT_SOURCE_DIR}/udc/udc_hal.c
        ${CMAKE_CURRENT_SOURCE_DIR}/udc/usb_dma.c
    )

    list(APPEND USB_SRCS ${USB_UDC_CSRCS})
endif()

# USB Gadget功能条件编译
if(CONFIG_USB_GADGET)
	target_include_directories(
		arch PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/gadget
	)
    # gadget源文件列表
    set(USB_GADGET_CSRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/gadget/gadget.c
        #${CMAKE_CURRENT_USB_PATH}/gadget/gadget_hal.c
    )

    list(APPEND USB_SRCS ${USB_GADGET_CSRCS})
endif()

# 注释保留原make.defs中的条件编译逻辑
# if(CONFIG_DRIVERS_USB_GADGET_ADB)
#     include_directories(${CMAKE_CURRENT_USB_PATH}/gadget)
#     list(APPEND USB_SRCS ${CMAKE_CURRENT_USB_PATH}/gadget/function/adb.c)
# endif()

target_sources(arch PRIVATE ${USB_SRCS})