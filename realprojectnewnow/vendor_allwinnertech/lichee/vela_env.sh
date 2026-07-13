function cmdls() {
	echo "\
csdk     
cboot     
cconfigs  
cdsp      
caw       
cnuttx    
cout      
cbin      
cchip     
cboard    
ctarget   
cdriver   
chal      
mtee 		-> make tee
	[c] 	-> make distclean
	[menuconfig]  -> make menuconfig
mbl 		-> make bl
	[c] 	-> make distclean
	[menuconfig]  -> make menuconfig
map 		-> make ap
	[c] 	-> make ap distclean
	[p] 	-> make ap and pack
	[menuconfig]  -> make menuconfig
mota      -> make ota
mall      -> make bl,ap,ota 
backtrace [elf] <addr> 	-> print backtrace
myobjdump <core dump file> -> coredump file
mygdb 		-> same as gdb
savelog   <file_name>
mysync    -> repo sync
catcmd    -> print cmds
"
}

function log() {
	echo "make:"$1
}
function csdk {
	cd $ROOT_PATH
}

function cboot {
	cd $aw_path/lichee/brandy-2.0/u-boot-2018/
}

function cconfigs {
	cd $aw_path/lichee/board/${RTOS_TARGET_PROJECT_PATH}/configs/
}

function cdsp {
	cd $aw_path/aw_dsp/
}

function caw {
	cd $aw_path
}

function cnuttx {
	cd $ROOT_PATH/nuttx
}

function cout {
	cd $aw_path/lichee/out/${RTOS_TARGET_PROJECT_PATH}/
}

function cbin {
	cd $aw_path/lichee/board/${RTOS_TARGET_PROJECT_PATH}/bin
}


function cchip {
	cd $aw_path/chips/${RTOS_TARGET_CHIPNAME}
}

function clvgl {
	cd $ROOT_PATH/apps/graphics/lvgl/lvgl
}

function cboard {
	cd $aw_path/boards/${RTOS_TARGET_CHIPNAME}/${RTOS_BOARD_DEVICE}
}

function ctarget {
	cd $aw_path/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE
}

function cdriver {
	cd $ROOT_PATH/nuttx/drivers
}

function chal {
	cchip
	cd drivers/rtos-hal/
}

# 编译bootlaoder镜像，只需要编译一次
function mbl() {
	case $1 in
		c)
			log "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/bootloader/ distclean"
			(\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/bootloader/ distclean)
			;;
		menuconfig)
			log "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/bootloader/ $1"
			(\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/bootloader/ $1)
			;;
		*)
			log "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/bootloader/ -e -Werror -j32"
			(\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/bootloader/  -e -Wno-error -j32)
			if [ $? != 0 ]; then
				ret=$?
				log "build_bl_failed"
				return $ret
			else
				log "build_bl_success"
				if [ "$(ps -p $$ -o comm=)" = "bash" ]; then
					if [ "$1" = "p" ]; then
						log "ap p -s"
						p -s
					fi
				fi
			fi
			;;
	esac
}

# $1 参数含义
# -j32       //编译ap镜像，即正常启动的系统，每次代码有更新都需要重新编译
# menuconfig //配置ap镜像的内容，可选
# distclean
function map() {
	case $1 in
		c)
			log "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/ap/ distclean"
			(\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/ap/ distclean)
			;;
		menuconfig)
			log "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/ap/ $1"
			(\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/ap/ $1)
			;;
		*)
			log "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/ap/  -e -Wno-error -v -j32"
			(\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/ap/ -e -Wno-error -j32)
			if [ $? != 0 ]; then
				ret=$?
				log "build_ap_failed"
				return $ret
			else
				log "build_ap_success"
				if [ "$(ps -p $$ -o comm=)" = "bash" ]; then
					if [ "$1" = "p" ]; then
						log "ap p -s"
						p -s
					fi
				fi
			fi
			;;
	esac
}

# 编译ota镜像，只需要编译一次
function mota() {
	case $1 in
		c)
			log "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/ota/ distclean"
			(\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/ota/ distclean)
			;;
		*)
			log "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/ota/  -e -Wno-error -j32"
			(\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/ota/ -e -Wno-error -j32)
			if [ $? != 0 ]; then
				log "build_ota_failed"
			else
				log "build_ota_success"
			fi
			;;
	esac
}

# 编译tee镜像
function mtee() {
	case $1 in
		c)
			log "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/tee/ distclean"
			(\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/tee/ distclean)
			;;
		*)
			log "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/tee/ -e -Wno-error -v -j32"
			(\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/tee/ -e -Wno-error -j32)
			if [ $? != 0 ]; then
				log "build_tee_failed"
			else
				log "build_tee_success"
			fi
			;;
	esac
}

function mnsh() {
	case $1 in
		c)
			log "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/nsh/ distclean"
			(\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/nsh/ distclean)
			;;
		*)
			log "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/nsh/ -e -Wno-error -v -j32"
			(\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/nsh/ -e -Wno-error -j32)
			if [ $? != 0 ]; then
				log "build_nsh_failed"
			else
				log "build_nsh_success"
			fi
			;;
	esac
}


function mall() {
	mbl $1 \
	&& mtee $1 \
	&& mota $1 \
	&& map $1
}

# 输入地址
function backtrace() {
	case $1 in
		ap|bl|ota|tee)
			$aw_toolchain/arm-none-eabi-addr2line -Capfe $ROOT_PATH/nuttx/vela_$1.elf $@
			;;
		file)
			# temp_file=$(mktemp)
			# # trap 'rm -f "$temp_file"' EXIT
			# while read -r line; do
				# if echo "$line" | grep -q 'backtrace|'; then
					# pid_tmp=$(echo "$line" | awk -F'|' '{print $2}')
					# pid=$(echo $pid_tmp | awk -F':' '{print $1}')
					# address=$(echo $pid_tmp | awk -F':' '{print $2}')
				# fi

				# # 将进程号和地址追加到临时文件中
				# echo "$pid,$address" >> "$temp_file"
			# done < "$3"
			# # 读取临时文件，按进程号分组，并调用自定义命令
			# current_pid=""
			# while read -r line; do
				# read pid address <<< $(echo "$line" | tr ',' ' ')
				# if [[ "$current_pid" != "$pid" ]]; then
					# if [[ -n "$current_pid" ]]; then
						# # 调用自定义命令，传递之前收集的地址列表
						# echo pid:"$current_pid"
							# echo addr1:$address
						# for addr in "${addresses[@]}"; do
							# echo addr:$addr
							# $aw_toolchain/arm-none-eabi-addr2line -Capfe $2 $addr
						# done
					# fi
					# # 重置变量
					# current_pid="$pid"
					# addresses=("$address")
				# else
					# # 追加地址到地址列表
					# addresses+=("$address")
				# fi
			# done < "$temp_file"
			# # 处理最后一个进程号
			# if [[ -n "$current_pid" ]]; then
				# echo pid:"$current_pid"
				# for addr in ${addresses[@]}; do
					# $aw_toolchain/arm-none-eabi-addr2line -Capfe $2 $addr
				# done
			# fi

			# echo $temp_file
			# # 移除临时文件
			# rm -f "$temp_file"
			$aw_toolchain/arm-none-eabi-addr2line -Capfe $2 $(paste -d ' ' $3)
			;;
		*)
			echo " plase input backtrace (ap|bl|ota|tee) addr,like backtrace ap <0x12345678> <0x12345678> ... "
			echo " or backtrace file xx.elf backtrace's file,like backtrace file myelf.elf backtrace.txt"
			;;
	esac
}

function coredump() {
	echo " ./coredump [.elf] [memdump.bin]"
	if [ -e $ROOT_PATH/../comm/coredump.py ]; then
		cp $ROOT_PATH/../comm/coredump.py $ROOT_PATH/nuttx/tools/coredump.py
	fi
	python3 $ROOT_PATH/nuttx/tools/coredump.py -e $1 -t $aw_toolchain/arm-none-eabi-gdb -r $2:0x40200000
}

function myobjdump() {
	$aw_toolchain/arm-none-eabi-objdump $@
}

function mygdb() {
	$aw_toolchain/arm-none-eabi-gdb $@
}

function savelog() {
	local log_time
	if [ $# = 1 ]; then
		log_time=`date +%Y%m%d%H%M%S`
	elif [ $# = 2 ]; then
		log_time=$2
	else
		echo "please input save file name"
		return 0
	fi
	local log_path=$ROOT_PATH/tags/$current_tag/logs/$1_$log_time
	mkdir -p ${log_path}
	cp $ROOT_PATH/nuttx/*.elf ${log_path}/
	cp $ROOT_PATH/out/rtos_nuttx_r528s3-x4b_uart0_secure_256Mnand_v0.img ${log_path}/
	cp $ROOT_PATH/out/image ${log_path}/ -a
}

function mysync() {
	ping -c 1 git.odm.mioffice.cn
	if [ $? != 0 ]; then
		echo "git.odm.mioffice.cn disconnected "
		return 0
	fi
	local REPO_TIME=`date +%Y-%m-%d-%H-%M-%S`
	if [ "$1" = "c" ]; then
		repo forall -c "git reset --hard;git clean -dxf"
	else
		repo forall -vc "git ck ."
	fi
	repo sync
	repo forall -vc "git tag tag-$REPO_TIME"
	echo "tag-$REPO_TIME" > .current_tag
	current_tag=tag-$REPO_TIME
	mkdir -p $ROOT_PATH/tags/$current_tag
	# 生成tag
	echo "repo forall -vc \"git tag -d tag-$REPO_TIME\";rm -rf $ROOT_PATH/tags/$current_tag" >> $ROOT_PATH/repo_time.sh
	# 生成根据 tag 切对应提交的脚本
	echo "#!/bin/bash" > $ROOT_PATH/tags/$current_tag/change_tag.sh
	echo "echo "repo time $REPO_TIME"" >> $ROOT_PATH/tags/${current_tag}/change_tag.sh
	repo forall -vc "\
		echo -n \"cd \" >> $ROOT_PATH/tags/${current_tag}/change_tag.sh; \
		pwd >> $ROOT_PATH/tags/${current_tag}/change_tag.sh; \
		echo -n \"git checkout \" >> $ROOT_PATH/tags/${current_tag}/change_tag.sh; \
		git log -1 --pretty=format:"%h" $current_tag >> $ROOT_PATH/tags/${current_tag}/change_tag.sh; \
		echo \"\" >> $ROOT_PATH/tags/${current_tag}/change_tag.sh; \
	" 
	# 取消uboot烧录时同时烧录rtopk, 需要主动重新编译uboot
	# (cboot && sed -i 's/^CONFIG_SUNXI_BURN_ROTPK_ON_SPRITE=y/# CONFIG_SUNXI_BURN_ROTPK_ON_SPRITE=y/' configs/sun8iw20p1_evb3_rtos_nand_defconfig && csdk)
	# 需要修改key.avb才能正常启动
	cp $ROOT_PATH/../comm/key.avb $ROOT_PATH/vendor/allwinnertech/boards/r528/r528s3-x4b/src/etc/
	mall c && mall
	if [ "$?" = "0" ] &&  [ "$(ps -p $$ -o comm=)" = "bash" ]; then
		if [ "$1" = "c" ]; then
			createkeys
			lunch_nuttx r528s3-x4b_tee
		fi
		p -s
		savelog "sync" $REPO_TIME
	fi
}

function catcmd() {
	cat $ROOT_PATH/../comm/test.sh
}