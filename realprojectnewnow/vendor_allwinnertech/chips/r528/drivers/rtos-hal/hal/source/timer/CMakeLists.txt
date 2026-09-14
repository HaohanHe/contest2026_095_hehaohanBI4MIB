if(CONFIG_DRIVERS_TIMER)
	list(APPEND SRCS hal_timer.c sunxi_timer.c)
endif()

if(CONFIG_DRIVERS_TIMER_AVS)
	list(APPEND SRCS hal_avs.c)
endif()

if(CONFIG_DRIVERS_HTIMER)
	list(APPEND SRCS hal_htimer.c sunxi_htimer.c)
endif()

# 为board_driver 目标完成配置，此处board_driver为上面定义驱动目标
target_sources(arch PRIVATE ${SRCS})
