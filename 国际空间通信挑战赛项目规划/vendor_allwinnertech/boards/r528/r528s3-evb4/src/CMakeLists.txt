set(SRCS
    r528_boot.c
    r528_leds.c
    r528_appinit.c
    r528_bringup.c
)

if(CONFIG_ETC_ROMFS)
    set(RCSRCS
        etc/init.d/rc.sysinit
        etc/init.d/rcS
    )
    set(RCRAWS
        etc/build.prop
        etc/factory.sh
    )

    if(CONFIG_X4B_AP)
        list(APPEND RCRAWS
            etc/EQ_AD51652.conf
            etc/EQ_ACM.conf
            etc/EQ_AD52058.conf
            etc/usr/share/mico
        )

        if(CONFIG_MICO_DEFAULT_TTS_PACK_INTO_FIRMWARE)
            list(APPEND RCRAWS
                etc/usr/share/sound/common
                etc/usr/share/sound/vendor
            )
        else()
            set(TTS_BASE_DIR "etc/usr/share/sound")
            set(TTS_COPY_DIR "${TOPDIR}/../vendor/allwinnertech/lichee/board/common/data/res")
            execute_process(
                COMMAND rm -rf ${TTS_COPY_DIR}/${TTS_BASE_DIR}
                COMMAND mkdir -p ${TTS_COPY_DIR}/${TTS_BASE_DIR}
                COMMAND cp -r ${TTS_BASE_DIR}/* ${TTS_COPY_DIR}/${TTS_BASE_DIR}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            )
        endif()
    endif()

    if(CONFIG_X4B_FACTEST)
        list(APPEND RCRAWS
            etc/EQ_AD51652.conf
            etc/EQ_ACM.conf
            etc/EQ_AD52058.conf
            etc/wifi_factest/rtl8733bs_wifi.map
            etc/wifi_factest/rtl8733bs_wifi.mask
        )
    endif()

	if(CONFIG_LIBC_LOCALTIME)
    	list(APPEND RCRAWS etc/zoneinfo)
	endif()

	if(CONFIG_SERVICES_MICO_ALARM)
   		list(APPEND RCRAWS etc/workday)
	endif()

	if(CONFIG_MIDESS)
    	list(APPEND RCRAWS etc/mi/ui_support.json)
	endif()

	if(CONFIG_MEDIA_SERVER)
    	list(APPEND RCRAWS etc/media)
	endif()

	if(CONFIG_COMPONENTS_BLUETOOTH)
    	if(CONFIG_X4B_FACTEST)
			list(APPEND RCRAWS etc/bt_factest)
		else()
			list(APPEND RCRAWS etc/bt)
		endif()
	endif()
	
	if(CONFIG_SERVICES_PNS)
		set(PNS_BASE_DIR "${TOPDIR}/../vendor/xiaomi/miai/pns/thirdparty/${CONFIG_PNS_PRODUCT_SERIES}")
		set(PNS_DST_DIR "etc/pns/configs/vpm/xiaomi")
		if(NOT CONFIG_PNS_LIBVPM_ENABLE STREQUAL "")
			execute_process(COMMAND rm -rf etc/pns WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
			execute_process(COMMAND mkdir -p ${PNS_DST_DIR} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
			execute_process(COMMAND cp -r ${PNS_BASE_DIR}/vpm/configs/xiaomi/* ${PNS_DST_DIR} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
			if(NOT CONFIG_PNS_CFG_PACK_INTO_FIRMWARE STREQUAL "")
				list(APPEND RCRAWS ${PNS_DST_DIR})
			else()
				set(PNS_COPY_DIR "${TOPDIR}/../vendor/allwinnertech/lichee/board/common/data/res")
				execute_process(COMMAND rm -rf ${PNS_COPY_DIR}/etc/pns WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
				execute_process(COMMAND mkdir -p ${PNS_COPY_DIR}/${PNS_DST_DIR} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
				if(EXISTS etc/pns)
					execute_process(COMMAND cp -r ${PNS_DST_DIR}/* ${PNS_COPY_DIR}/${PNS_DST_DIR} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
				else()
					execute_process(COMMAND cp -r ${PNS_BASE_DIR}/vpm/configs/xiaomi/* ${PNS_COPY_DIR}/${PNS_DST_DIR} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
				endif()
			endif()
		endif()
	endif()

	if(CONFIG_SYSTEM_PACKAGE_SERVICE)
		list(APPEND RCRAWS etc/package.cfg)
	endif()

	if(CONFIG_SYSTEM_ACTIVITY_SERVICE)
		list(APPEND RCRAWS etc/lmk.cfg)
	endif()
	
	nuttx_add_romfs(
	  NAME
	  etc
	  MOUNTPOINT
	  etc
	  RCSRCS
	  ${RCSRCS}
	  RCRAWS
	  ${RCRAWS}
	  PATH
	  ${CMAKE_CURRENT_LIST_DIR}/etc)

	target_link_libraries(board PRIVATE romfs_etc)
	
endif()

target_sources(board PRIVATE ${SRCS})
