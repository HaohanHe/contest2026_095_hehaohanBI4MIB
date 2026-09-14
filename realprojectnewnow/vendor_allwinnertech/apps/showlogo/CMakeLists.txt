# ##############################################################################
# vendor/vendor_name/apps/vendor_app/CMakeLists.txt
#
# copyright
#
# ##############################################################################



if(CONFIG_SHOW_LOGO)
nuttx_add_application(
    NAME                                #参数标志：application唯一名称
    showlogo 							#参数值：  取 Kconfig中的设置值应用名称
    SRCS                                #参数标志：源文件
    showlogo.c                          #参数值：  传入应用的源文件，可以多个，main必须为第一个
    STACKSIZE                           #参数标志：栈大小
    ${CONFIG_SHOW_LOGO_STACKSIZE} 		#参数值：  取Kconfig中的设置值，不传则为CONFIG_DEFAULT_TASK_STACKSIZE
    PRIORITY                            #参数标志：任务优先级
    ${CONFIG_SHOW_LOGO_PRIORITY}) 		#参数值：  取Kconfig中的设置值，不传则为SCHED_PRIORITY_DEFAULT
endif()
