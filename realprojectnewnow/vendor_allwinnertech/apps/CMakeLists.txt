# 添加当前目录，即vendor_name/apps/下的全部子目录进入应用编译
nuttx_add_subdirectory()

# 也可根据需求添加单个目录
# add_subdirectory(my_app)

# 标记生成应用目录的 Kconfig MENU
# 多层目录可以递归生成，需要写在add_subdirctory()或nuttx_add_subdirectory()之后
nuttx_generate_kconfig(MENUDESC "Vendor_Name APPS")
