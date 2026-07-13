function hmm() {
cat <<EOF
Invoke ". envsetup.sh" from your shell to add the following functions to your environment:

== build project ==
- mboot:        Build boot0 and uboot, including uboot for nor.
- mboot0:       Just build boot0.
- muboot:       Build uboot, including uboot for nor.

== jump directory ==
- croot:    Jump to the top of the tree.
- cboot:    Jump to uboot.
- cboot0:   Jump to boot0.
- cbin:     Jump to uboot/boot0 bin directory.
- cconfigs: Jump to configs of target.
- cout:     Jump to out directory of target.

Look at the source to view more functions. The complete list is:
EOF
    T=$(gettop)
    local A
    A=""
    for i in `cat $T/tools/scripts/envsetup.sh | sed -n "/^[ \t]*function /s/function \([a-z_]*\).*/\1/p" | sort | uniq`; do
        A="$A $i"
    done
    echo $A
}

function gettop()
{
    local TOPFILE=tools/scripts/envsetup.sh
    if [ -n "$RTOS_TOP" -a -f "$RTOS_TOP/$TOPFILE" ] ; then
        # The following circumlocution ensures we remove symlinks from TOP.
        (\cd $RTOS_TOP; PWD= /bin/pwd)
    else
        if [ -f $TOPFILE ] ; then
            # The following circumlocution (repeated below as well) ensures
            # that we record the true directory name and not one that is
            # faked up with symlink names.
            PWD= /bin/pwd
        else
            local here="${PWD}"
            while [ "${here}" != "/" ]; do
                if [ -f "${here}/${TOPFILE}" ]; then
                    (\cd ${here}; PWD= /bin/pwd)
                    break
                fi
                here="$(dirname ${here})"
            done
        fi
    fi
}

function setup_toolchain()
{
    if [ "`uname`" == "Darwin" ]; then
        export MACOSX_DEPLOYMENT_TARGET=11
        echo -e "Note: macOS users need to manually deploy toolchains. Skipping prebuilts setup."
        return
    fi

    ARCH=(\
        "xtensa" \
        "arm" \
        "risc-v" \
    "tc32" )

    TOOLCHAIN=(\
        "gcc" \
    "clang" )

    if [ "$XTENSAD_LICENSE_FILE" == "" ]; then
        export XTENSAD_LICENSE_FILE=28000@10.221.64.91
    fi
    export WASI_SDK_ROOT=${ROOTDIR}/prebuilts/clang/linux/wasm
    export PYTHONPATH=${PYTHONPATH}:${ROOTDIR}/prebuilts/tools/python/dist-packages/pyelftools
    export PYTHONPATH=${PYTHONPATH}:${ROOTDIR}/prebuilts/tools/python/dist-packages/cxxfilt

    for (( i = 0; i < ${#ARCH[*]}; i++)); do
        for (( j = 0; j < ${#TOOLCHAIN[*]}; j++)); do
            export PATH=${ROOTDIR}/prebuilts/${TOOLCHAIN[$j]}/linux/${ARCH[$i]}/bin:$PATH
        done
    done

    # Arm Compiler
    export PATH=${ROOTDIR}/prebuilts/clang/linux/armclang/bin:$PATH

    if [ ! -n "${ARM_PRODUCT_DEF}" ]; then
        export ARM_PRODUCT_DEF=${ROOTDIR}/prebuilts/clang/linux/armclang/mappings/eval.elmap
    fi
    if [ ! -n "${LM_LICENSE_FILE}" ]; then
        export LM_LICENSE_FILE=${HOME}/.arm/ds/licenses/DS000-EV-31030.lic
    fi
    if [ ! -n "${ARMLMD_LICENSE_FILE}" ]; then
        export ARMLMD_LICENSE_FILE=${HOME}/.arm/ds/licenses/DS000-EV-31030.lic
    fi

    # Generate compile database file compile_commands.json
    BEAR_DIR="${ROOTDIR}/prebuilts/tools/bear/bin/bear"
    baer="${BEAR_DIR} --output compile_commands.json --append --"
    export PATH="${BEAR_DIR}:$PATH"

    # Add compile cache
    CCACHE_DIR=${ROOTDIR}/prebuilts/tools/ccache
    #ln -sf ${CCACHE_DIR}/bin/ccache ${CCACHE_DIR}/cc
    #ln -sf ${CCACHE_DIR}/bin/ccache ${CCACHE_DIR}/c++
    #ln -sf ${CCACHE_DIR}/bin/ccache ${CCACHE_DIR}/gcc
    #ln -sf ${CCACHE_DIR}/bin/ccache ${CCACHE_DIR}/g++
    #ln -sf ${CCACHE_DIR}/bin/ccache ${CCACHE_DIR}/clang
    #ln -sf ${CCACHE_DIR}/bin/ccache ${CCACHE_DIR}/clang++
    #ln -sf ${CCACHE_DIR}/bin/ccache ${CCACHE_DIR}/arm-none-eabi-gcc
    #ln -sf ${CCACHE_DIR}/bin/ccache ${CCACHE_DIR}/arm-none-eabi-g++
    #ln -sf ${CCACHE_DIR}/bin/ccache ${CCACHE_DIR}/riscv64-unknown-elf-gcc
    #ln -sf ${CCACHE_DIR}/bin/ccache ${CCACHE_DIR}/riscv64-unknown-elf-g++
    export PATH="${CCACHE_DIR}:$PATH"

    # add the tools for vela prebuilt tools
    SYSTEM=`uname | tr '[:upper:]' '[:lower:]'`
    SYS_ARCH=`uname -m | sed 's/arm64/aarch64/'`
    TOOLS_DIR=${ROOTDIR}/prebuilts/tools/${SYSTEM}/${SYS_ARCH}
    export PATH="${TOOLS_DIR}:$PATH"
}

function envsetup
{
    if [ "x$SHELL" != "x/bin/bash" ]; then
        case `ps -o command -p $$` in
            *bash*)
            ;;
            *)
                echo -n "WARNING: Only bash is supported, "
                echo "use of other shell would lead to erroneous results"
            ;;
        esac
    fi

    # check top of SDK
    if [ ! -f "${PWD}/tools/scripts/envsetup.sh" ]; then
        echo "ERROR: Please source envsetup.sh in the root of SDK"
        return -1
    else
        export RTOS_TOP="$(PWD= /bin/pwd)"
    fi

    export TARGET_BUILD_VARIANT=nuttx
    export TARGET_BUILD_RTOS=nuttx

    export ROOTDIR=$(realpath $(gettop)/../../../)
    export NUTTXDIR=${ROOTDIR}/nuttx
    export TOOLSDIR=${NUTTXDIR}/tools
    export OUTDIR=${ROOTDIR}
    export ROOT_PATH=${ROOTDIR}
    # echo ROOT_PATH $ROOT_PATH

    if [ ! -f "${ROOTDIR}/prebuilts/kconfig-frontends/bin/kconfig-conf" ] &&
    [ ! -x "$(command -v kconfig-conf)" ]; then
        pushd ${ROOTDIR}/prebuilts/kconfig-frontends
        ./configure --prefix=${ROOTDIR}/prebuilts/kconfig-frontends 1>/dev/null
        touch aclocal.m4 Makefile.in
        make install 1>/dev/null
        popd
    fi
    export PATH=${ROOTDIR}/prebuilts/kconfig-frontends/bin:$PATH

    setup_toolchain

    get_all_projects
    complete -F _lunch lunch

    echo "Setup env done!"
    echo -e "Run \033[32mlunch_nuttx\033[0m to select project"
}

function p
{
    local T=$(gettop)
    [ -z "$T" ] \
    && echo "Couldn't locate the top of the tree.  Try setting TOP." \
    && return

    (\cd $T && pack $@)
}

function getbindir()
{
    if [ "x${RTOS_TARGET_CHIP}" == "xsun20iw2p1" ]; then
        local BIN_DIR="board/${RTOS_TARGET_BOARD_PATH%_*}/bin"
    else
        local BIN_DIR="board/${RTOS_TARGET_BOARD_PATH}/bin"
    fi
    echo ${BIN_DIR}
}

# Build brandy(uboot,boot0,fes) if you want.
function build_boot()
{
    local T=$(gettop)
    local chip=${RTOS_TARGET_CHIP}
    local cmd=$1
    local o_option=$2
    local platform
    local bin_dir=$(getbindir)
    local special_config=""
    export LICHEE_FLASH="default"

    if [ "x$chip" = "x" ]; then
        echo "platform($RTOS_TARGET_PROJECT%%_*) not support"
        return 1
    fi

    if [ -f "$T/board/${RTOS_TARGET_BOARD_PATH%_*}/configs/BoardConfig.mk" ]; then
        special_config="$T/board/${RTOS_TARGET_BOARD_PATH%_*}/configs/BoardConfig.mk"
        elif [ -f "$T/board/${RTOS_TARGET_BOARD_PATH}/configs/BoardConfig.mk" ]; then
        special_config="$T/board/${RTOS_TARGET_BOARD_PATH}/configs/BoardConfig.mk"
    fi

    if [ x"$o_option" = x"uboot" ]; then
        platform=${chip}_evb3_rtos_nand
        if [ x"${special_config}" != x"" ]; then
            platform=$(grep "LICHEE_BRANDY_DEFCONF" ${special_config} | awk -F "=" '{print $2}')
            platform=${platform%%_def*}
        fi
    else
        platform=${chip}
    fi

    \cd $T/brandy-2.0/
    mkdir -p $T/${bin_dir}

    if [ x"$o_option" == "xboot0" ]; then
        o_option=spl
    fi

    echo "build_boot platform:$platform o_option:$o_option"
    if [ x"$o_option" != "x" ]; then
        echo bin_dir ${bin_dir}
        TARGET_BIN_DIR=${bin_dir} ./build.sh -p $platform -o $o_option
    else
        TARGET_BIN_DIR=${bin_dir} ./build.sh -p $platform
    fi
    if [ $? -ne 0 ]; then
        echo "$cmd stop for build error in brandy, Please check!"
        \cd - 1>/dev/null
        return 1
    fi
    \cd - 1>/dev/null
    echo "$cmd success!"
    return 0
}

function muboot
{
    (build_boot muboot uboot)
}

function mboot
{
    (build_boot muboot uboot)
    (build_boot mboot0 boot0)
}

function mboot0
{
    (build_boot mboot0 boot0)
}

pack_usage()
{
    printf "Usage: pack [-cCHIP] [-pPLATFORM] [-bBOARD] [-oOS] [-fPROJECT_PATH] [-gBOARD_PATH] [-s] [-m] [-w] [-i] [-h]
	-c CHIP (default: $chip)
	-p PLATFORM (default: $platform)
	-b BOARD (default: $board)
	-s pack firmware with signature
	-m pack dump firmware
	-w pack programmer firmware
	-i pack sys_partition.fex downloadfile img.tar.gz
	-h print this help message
	-f project path
	-g board path
    "
}

function pack() {
    local T=$(gettop)
    local chip=${RTOS_TARGET_CHIP}
    local platform=rtos

    if [ "x${RTOS_TARGET_CHIP}" == "xsun20iw2p1" ]; then
        local project_path=${RTOS_TARGET_PROJECT_PATH%_*}
        local board_path=${RTOS_TARGET_BOARD_PATH%_*}
        local board=${RTOS_PROJECT_NAME%_*}
    else
        local project_path=${RTOS_TARGET_PROJECT_PATH}
        local board_path=${RTOS_TARGET_BOARD_PATH}
        local board=${RTOS_PROJECT_NAME%_*}
    fi

    local debug=uart0
    local sigmode=none
    local securemode=none
    local mode=normal
    local programmer=none
    local tar_image=none
    local os=${TARGET_BUILD_RTOS}
    local hostos=linux
    unset OPTIND
    while getopts "dsvmwih" arg
    do
        case $arg in
            s)
                sigmode=secure
            ;;
            v)
                securemode=secure
            ;;
            m)
                mode=dump
            ;;
            w)
                programmer=programmer
            ;;
            i)
                tar_image=tar_image
            ;;
            h)
                pack_usage
                return 0
            ;;
            ?)
                return 1
            ;;
        esac
    done

    chip=${RTOS_TARGET_CHIP}

    if [ "x$chip" = "x" ]; then
        echo "platform($RTOS_PROJECT_NAME%%_*) not support"
        return
    fi

    $T/tools/scripts/pack_img.sh -c $chip -p $platform -b $board -o $os \
    -d $debug -s $sigmode -m $mode -w $programmer -v $securemode -i $tar_image -t $T -f $project_path -g $board_path
}

function createkeys()
{
    local T=$(gettop)
    $T/tools/scripts/createkeys
}

function croot()
{
    T=$(gettop)
    local T=$(gettop)
    [ -z "$T" ] \
    && echo "Couldn't locate the top of the tree.  Try setting TOP." \
    && return
    \cd $T
}

function cboot()
{
    local T=$(gettop)
    [ -z "$T" ] \
    && echo "Couldn't locate the top of the tree.  Try setting TOP." \
    && return

    \cd $(gettop)/brandy-2.0/u-boot-2018
}

function cboot0()
{
    local T=$(gettop)
    [ -z "$T" ] \
    && echo "Couldn't locate the top of the tree.  Try setting TOP." \
    && return

    \cd $T/brandy-2.0/spl/
}

function cbsp()
{
    local T=$(gettop)
    [ -z "$T" ] \
    && echo "Couldn't locate the top of the tree.  Try setting TOP." \
    && return

    \cd $T/../chips/r528/drivers/rtos-hal/hal/source/
}

function cosal()
{
    local T=$(gettop)
    [ -z "$T" ] \
    && echo "Couldn't locate the top of the tree.  Try setting TOP." \
    && return

    \cd $T/../chips/r528/drivers/osal/src/
}


function cprojects()
{
    local T=$(gettop)
    [ -z "$T" ] \
    && echo "Couldn't locate the top of the tree.  Try setting TOP." \
    && return

    if [ -d "$T/lichee/rtos/projects/${RTOS_TARGET_PROJECT_PATH}/"  ]; then
        \cd $T/lichee/rtos/projects/${RTOS_TARGET_PROJECT_PATH}/
        return
    fi

    \cd $T/../boards/r528/$RTOS_TARGET_DEVICE/configs/${RTOS_PROJECT_CONFIG_PATH}

}

function print_lunch_menu()
{
    local uname=$(uname)
    echo
    echo "You're building on" $uname
    echo
    echo "Lunch menu... pick a combo:"

    local i=1
    local choice
    for choice in ${auto_complete_opts[@]}
    do
        echo "     $i. $choice"
        i=$(($i+1))
    done
    echo
}

function get_all_projects()
{
    local T=$(gettop)
    local exit_flag=0;

    if [ "${#auto_complete_opts[@]}" -ne 0 ]; then
        unset auto_complete_opts
    fi

    if [ "${#device_name_opts[@]}" -ne 0 ]; then
        unset device_name_opts
    fi

    if [ "${#chip_name_opts[@]}" -ne 0 ]; then
        unset chip_name_opts
    fi

    for f1 in `ls -l ${T}/../boards/ | awk '/^d/{print $NF}'`;
    do
        for f in `ls -l ${T}/../boards/${f1} | awk '/^d/{print $NF}'`;
        do
            if [ "${f}" == "build"  ]; then
                continue;
            fi

            if [ "${f}" == "drivers"  ]; then
                continue;
            fi
            exit_flag=0;
            local project_name=${f};

            #for n in `ls -l ${T}/../boards/${f1}/${f}/configs/ | awk '/^d/{print $NF}'`;
            #do
            #	project_name=${f}_${n}
            #	for i in ${auto_complete_opts[@]}
            #	do
            #		if [ "${i}" == "${project_name}" ]; then
            #			exit_flag=1;
            #			echo i=${i}
            #			break;
            #		fi
            #	done
            if [ ${exit_flag} -eq 0 ]; then
                auto_complete_opts=(${auto_complete_opts[@]} ${project_name})
                device_name_opts=(${device_name_opts[@]} ${f})
                chip_name_opts=(${chip_name_opts[@]} ${f1})
            fi
            #done
        done
    done

    #search project's soft link
    for f1 in `ls -l ${T}/../boards/ | awk '/^l/{print $(NF-2)}'`;
    do
        for f in `ls -l ${T}/../boards/${f1} | awk '/^d/{print $NF}'`;
        do
            exit_flag=0;
            local project_name=${f};
            #for n in `ls -l ${T}/../boards/${f1}/${f}/configs/ | awk '/^d/{print $NF}'`;
            #do
            #	project_name=${f}-${n}
            #	for i in ${auto_complete_opts[@]}
            #	do
            #		if [ "${i}" == "${project_name}" ]; then
            #			exit_flag=1;
            #			echo i=${i}
            #			break;
            #		fi
            #	done
            if [ ${exit_flag} -eq 0 ]; then
                auto_complete_opts=(${auto_complete_opts[@]} ${project_name})
                device_name_opts=(${device_name_opts[@]} ${f})
                chip_name_opts=(${chip_name_opts[@]} ${f1})
            fi
            #done
        done
    done
}

function get_sub_projects()
{
    local T=$(gettop)

    if [ "${#auto_complete_sub_opts[@]}" -ne 0 ]; then
        unset auto_complete_sub_opts
    fi

    for f1 in `ls -l ${T}/../boards/$RTOS_TARGET_CHIPNAME/$1/configs | awk '/^d/{print $NF}'`;
    do
        #echo $f1
        auto_complete_sub_opts=(${auto_complete_sub_opts[@]} ${f1})
    done
}

function _lunch() {
    local cur prev

    COMPREPLY=()

    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    if [[ ${cur} == * && "${auto_complete_opts}" != *"${prev}"* ]] ; then
        COMPREPLY=( $(compgen -W "${auto_complete_opts}" -- ${cur})  )
        return 0
    fi
}

function lunch_nuttx()
{
    local T="$(gettop)"
    local last
    local choice
    local i=1

    # select platform
    local select
    if [ "$1" ] ; then
        select=$1
    else
        print_lunch_menu
        echo -n "Which would you like?"
        echo -n ": "
        cd $ROOT_PATH
        read select
        cd -
    fi

    if [ -z "${select}" ]; then
        select="${last}"
        elif (echo -n $select | grep -q -e "^[0-9][0-9]*$"); then
        if [ $select -le ${#auto_complete_opts[@]} ]; then
            select=${auto_complete_opts[$(($select-1))]}
        else
            echo "Invalid lunch combo: $select" >&2
            return 1
        fi
    fi

    local found_flag=0;
    for choice in ${auto_complete_opts[@]}
    do
        if [ "${select}" == "${choice}" ]; then
            export RTOS_TARGET_DEVICE=${device_name_opts[$(($i-1))]}
            export RTOS_TARGET_CHIPNAME=${chip_name_opts[$(($i-1))]}
            found_flag=1
            break;
        fi
        i=$(($i+1))
    done

    if [ ${found_flag} -eq 0 ]; then
        echo "Invalid lunch combo: $select"
        return 1
    fi

    export RTOS_PROJECT_NAME=${select}
    export RTOS_TARGET_PROJECT_PATH=${RTOS_TARGET_DEVICE/-//}_nand
    export RTOS_TARGET_BOARD_PATH=${RTOS_TARGET_DEVICE/-//}_nand
    export LICHEE_CHIP_CONFIG_DIR=${T}/board/${RTOS_TARGET_BOARD_PATH}/

    . ${T}/../boards/${RTOS_TARGET_CHIPNAME}/.rtos_config

    #	. ${T}/board/${RTOS_TARGET_PROJECT_PATH}/configs/BoardConfig.mk

    get_sub_projects ${select}

    export RTOS_PROJECT_CONFIG_PATH=${auto_complete_sub_opts[$((0))]}
    export RTOS_CONFIG_PATH=${T}/../boards/${RTOS_TARGET_CHIPNAME}/${RTOS_TARGET_DEVICE}/configs/${RTOS_PROJECT_CONFIG_PATH}
    export RTOS_BOARD_DEVICE=$RTOS_PROJECT_NAME

    # echo RTOS_PROJECT_NAME $RTOS_PROJECT_NAME
    # echo RTOS_TARGET_PROJECT_PATH $RTOS_TARGET_PROJECT_PATH
    # echo RTOS_TARGET_BOARD_PATH $RTOS_TARGET_BOARD_PATH
    # echo LICHEE_CHIP_CONFIG_DIR $LICHEE_CHIP_CONFIG_DIR
    # echo RTOS_PROJECT_CONFIG_PATH $RTOS_PROJECT_CONFIG_PATH
    # echo RTOS_CONFIG_PATH $RTOS_CONFIG_PATH

    # if ! ${TOOLSDIR}/configure.sh -e ${RTOS_CONFIG_PATH}; then
    #     echo "Error: ############# config ${1} fail ##############"
    #     return 1
    # fi

    source ${T}/vela_env.sh

    local image_count=0;

    for choice in ${auto_complete_sub_opts[@]}
    do
        image_count=$(($image_count+1))
    done

    for choice in ${auto_complete_sub_opts[@]}
    do
        if [ ${image_count} -eq 1 ]; then
            echo "run \"m or m ${choice} or mnuttx ${choice}\" to build ${choice}; \"m ${choice} c\" to clean ${choice}"
        else
            echo "run \"m ${choice} or mnuttx ${choice}\" to build ${choice}; \"m ${choice} c\" to clean ${choice}"
        fi

    done

    echo run \"pack\" to pack all image;
}

#compile compile ap,ota,bootloader project at once
function mnuttx_all()
{
    local T="$(gettop)"
    local AP_CONFIG_PATH=vendor/allwinnertech/boards/${RTOS_TARGET_CHIPNAME}/${RTOS_TARGET_DEVICE}/configs/ap
    local OTA_CONFIG_PATH=vendor/allwinnertech/boards/${RTOS_TARGET_CHIPNAME}/${RTOS_TARGET_DEVICE}/configs/ota
    local BL_CONFIG_PATH=vendor/allwinnertech/boards/${RTOS_TARGET_CHIPNAME}/${RTOS_TARGET_DEVICE}/configs/bootloader
    local NSH_CONFIG_PATH=vendor/allwinnertech/boards/${RTOS_TARGET_CHIPNAME}/${RTOS_TARGET_DEVICE}/configs/nsh

    echo "ap:${AP_CONFIG_PATH}"
    echo "ota:${OTA_CONFIG_PATH}"
    echo "bootloader:${BL_CONFIG_PATH}"
    echo "nsh:${NSH_CONFIG_PATH}"

    \cd $T/../../../
    [ -d ${AP_CONFIG_PATH} ] && {
        print_red "=== compile ap ==="
        ./build.sh ${AP_CONFIG_PATH} -j$(nproc)
        [ $? -ne 0 ] \
        && echo "**********make ap fail***********" \
        && return 1
        # echo "copy nuttx.bin to $T/board/${RTOS_TARGET_BOARD_PATH}/configs/ap.fex"
        # cp ${NUTTXDIR}/nuttx.bin ${T}/board/${RTOS_TARGET_BOARD_PATH}/configs/ap.fex
    }

    [ -d ${OTA_CONFIG_PATH} ] && {
        print_red "=== compile ota ==="
        ./build.sh ${OTA_CONFIG_PATH} -j$(nproc)
        [ $? -ne 0 ] \
        && echo "**********make ota fail***********" \
        && return 1
        # echo "copy nuttx.bin to $T/board/${RTOS_TARGET_BOARD_PATH}/configs/ota.fex"
        # cp ${NUTTXDIR}/nuttx.bin ${T}/board/${RTOS_TARGET_BOARD_PATH}/configs/ota.fex
    }

    [ -d ${BL_CONFIG_PATH} ] && {
        print_red "=== compile bootloader ==="
        ./build.sh ${BL_CONFIG_PATH} -j$(nproc)
        [ $? -ne 0 ] \
        && echo "**********make bootloader fail***********" \
        && return 1
        # echo "copy nuttx.bin to $T/board/${RTOS_TARGET_BOARD_PATH}/configs/bl.fex"
        # cp ${NUTTXDIR}/nuttx.bin ${T}/board/${RTOS_TARGET_BOARD_PATH}/configs/bl.fex
    }

    [ -d ${BL_CONFIG_PATH} ] && {
        print_red "=== compile nsh ==="
        ./build.sh ${NSH_CONFIG_PATH} -j$(nproc)
        [ $? -ne 0 ] \
        && echo "**********make nsh fail***********" \
        && return 1
        # echo "copy nuttx.bin to $T/board/${RTOS_TARGET_BOARD_PATH}/configs/nsh.fex"
        # cp ${NUTTXDIR}/nuttx.bin ${T}/board/${RTOS_TARGET_BOARD_PATH}/configs/nsh.fex
    }
    \cd -

    #nuttx/.config has been changed, so config again.
    # if ! ${TOOLSDIR}/configure.sh -e ${RTOS_CONFIG_PATH}; then
    #     echo "Error: ############# config ${1} fail ##############"
    #     return
    # fi
    return 0
}

function mnsh() {
    case $1 in
        c)
            log "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/nsh/ distclean"
            (\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/nsh/ distclean)
        ;;
        *)
            log "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/nsh/ -e -Wno-error -v -j$(nproc)"
            (\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/nsh/ -e -Wno-error -j$(nproc))
            if [ $? != 0 ]; then
                log "build_nsh_failed"
            else
                log "build_nsh_success"
            fi
        ;;
    esac
}

function mnuttx()
{
    local select_image=;
    local do_clean=0;
    local do_found=0;
    if [ -z "$1" ]; then
        select_image=$auto_complete_sub_opts
        do_found=1;
    else
        if [ "c" == $1 ]; then
            do_clean=1;
            select_image=$auto_complete_sub_opts
            do_found=1;
        fi

        if [ ${do_found} -eq 0 ]; then
            for choice in ${auto_complete_sub_opts[@]}
            do
                if [ "$1" == "${choice}" ]; then
                    do_found=1;
                    select_image=$1
                fi
            done

            if [ ${do_clean} -eq 0 ]; then
                if [ -n "$2" ] && [ "$2" == "c" ]; then
                    do_clean=1;
                fi
            fi
        fi
    fi

    if [ ${do_found} -eq 0 ]; then
        echo "vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/$1/ not found"
        return 0
    fi

    if [ ${do_clean} -eq 1 ]; then
        echo "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/$select_image/ distclean"
        (\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/$select_image/ distclean)
    else
        echo "./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/$select_image/ -e -Wno-error -v -j$(nproc)"
        (\cd $ROOT_PATH && ./build.sh vendor/allwinnertech/boards/$RTOS_TARGET_CHIPNAME/$RTOS_BOARD_DEVICE/configs/$select_image/ -e -Wno-error -j$(nproc))
        if [ $? != 0 ]; then
            echo "build $select_image failed"
        else
            echo "build $select_image success"
            #cp ${NUTTXDIR}/nuttx.bin ${T}/board/${RTOS_TARGET_BOARD_PATH}/bin/freertos.fex
            #cp ${NUTTXDIR}/nuttx.bin ${T}/board/${RTOS_TARGET_BOARD_PATH}/bin/$select_image.fex
        fi
    fi
}

function m()
{
    mnuttx $@
}

function mnuttx_menuconfig()
{
    local T="$(gettop)"

    cd ${T}/../../..; ./build.sh ${RTOS_CONFIG_PATH} menuconfig

    cd -
}

function ota_mnuttx_menuconfig()
{
    local T="$(gettop)"

    if [ ! -d ${RTOS_CONFIG_PATH} ]; then
        cp ${ROOTDIR}/nuttx/boards/*/*/${1/[:|\/]//configs/d}/ota_defconfig ${NUTTXDIR}/.config
    else
        cp  ${RTOS_CONFIG_PATH}/ota_defconfig ${NUTTXDIR}/.config
    fi

    cd ${T}/../../../nuttx; make menuconfig
    if [ $? -eq 0 ]; then
        if [ ! -d ${RTOS_CONFIG_PATH} ]; then
            cp ${NUTTXDIR}/.config ${ROOTDIR}/nuttx/boards/*/*/${1/[:|\/]//configs/d}/ota_defconfig
        else
            cp ${NUTTXDIR}/.config ${RTOS_CONFIG_PATH}/ota_defconfig
        fi
    fi
}

function ota_mnuttx()
{
    local T="$(gettop)"

    if [ ! -d ${RTOS_CONFIG_PATH} ]; then
        cp ${ROOTDIR}/nuttx/boards/*/*/${1/[:|\/]//configs/d}/ota_defconfig ${NUTTXDIR}/.config
    else
        cp  ${RTOS_CONFIG_PATH}/ota_defconfig ${NUTTXDIR}/.config
    fi

    if [ 1 -eq 1 ]; then
        local JOBS=`grep -c ^processor /proc/cpuinfo`

        (cd ${T}/../../../nuttx && make -j${JOBS} EXTRAFLAGS="$EXTRA_FLAGS" $@ )
        [ $? -ne 0 ] \
        && echo "**********make nuttx fail***********" \
        && return 1
    else
        (cd ${T}/../../../nuttx/ && make EXTRAFLAGS="$EXTRA_FLAGS" $@)
        [ $? -ne 0 ] \
        && echo "**********make nuttx fail***********" \
        && return 1
    fi

    if [ "${1}" == "distclean"  ]; then
        return 0
    fi

    echo "copy recovery binary to board folder ..."
    cp ${NUTTXDIR}/nuttx.bin ${T}/board/${RTOS_TARGET_BOARD_PATH}/bin/recovery.fex
}
function cnuttx()
{
    T=$(gettop)
    local T=$(gettop)
    [ -z "$T" ] \
    && echo "Couldn't locate the top of the tree.  Try setting TOP." \
    && return
    \cd $T/../../../nuttx
}

function cvendor()
{
    T=$(gettop)
    local T=$(gettop)
    [ -z "$T" ] \
    && echo "Couldn't locate the top of the tree.  Try setting TOP." \
    && return
    \cd $T/../
}

function mlib()
{
    echo "create lib ..."
    local T=$(gettop)

    if [ $# != 2  ] ; then
        echo "USAGE: mlib <path> <libname>"
        echo " e.g.: mlib vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source/nand_flash/ libnuttx_sun8iw20p1_nandflash.a"
        return 1;
    fi

    [ -z "$T" ] \
    && echo "Couldn't locate the top of the tree.  Try setting TOP." \
    && return

    cd $1

    [ $? -ne 0 ] \
    && echo "**********create lib fail***********" \
    && return 1

    ALL_OBJECTS_FILES="$(find . -type f -iname '*.o' | grep -v 'obj-in.o')"

    if [ "${RTOS_TARGET_ARCH}" == "arm"  ];then
        arm-none-eabi-ar rv -o $2 ${ALL_OBJECTS_FILES}
    fi
    [ $? -eq 0 ] && echo -e "\033[31m make ${1}/${2} successfully \033[0m"
    cd -
}

function doobjdump()
{
    T=$(gettop)
    local T=$(gettop)
    [ -z "$T" ] \
    && echo "Couldn't locate the top of the tree.  Try setting TOP." \
    && return
    arm-none-eabi-objdump -d $T/../../../nuttx/nuttx
}

function print_red()
{
    echo -e '\033[0;31;1m'
    echo $1
    echo -e '\033[0m'
}

[ -e ./tools/scripts/.hooks/expand_func ] &&
source ./tools/scripts/.hooks/expand_func

EXTRA_FLAGS="-Wno-cpp"

envsetup
