if(CONFIG_DRIVERS_MSGBOX)
	target_include_directories(
     	arch PRIVATE
 		${CMAKE_CURRENT_SOURCE_DIR}
	)
    list(APPEND SRCS msgbox_amp.c)
endif()
target_sources(arch PRIVATE ${SRCS})
