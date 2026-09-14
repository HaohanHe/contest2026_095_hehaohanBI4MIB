#!/bin/bash

# Configuration
TOPDIR="$(cd "$(dirname "$0")/../../../nuttx" && pwd)"
VENDOR_DIR="$(cd "$(dirname "$0")/.." && pwd)"
LIBEXT=".a"

ARCH_LIB_SRC="${TOPDIR}/arch/arm/src/libarch${LIBEXT}"
BOARD_LIB_SRC="${VENDOR_DIR}/boards/r528/r528s3-gemini-s1/src/libboard${LIBEXT}"

ARCH_LIB_DST="${VENDOR_DIR}/libs/r528/libarch_allwinner${LIBEXT}"
BOARD_LIB_DST="${VENDOR_DIR}/libs/board/r528s3-gemini-s1/libboard_allwinner${LIBEXT}"
DRIVER_LIB_DST="${VENDOR_DIR}/libs/drivers/r528/libdrivers_r528${LIBEXT}"

echo "Allwinnertech Library Release Automation"
echo "========================================"

# Check if we are in the correct environment
if [ ! -f "${TOPDIR}/.config" ]; then
    echo "Error: NuttX .config not found in ${TOPDIR}"
    exit 1
fi

# Check if source mode is active
if grep -q "CONFIG_ALLWINNER_BINARY_ONLY=y" "${TOPDIR}/.config"; then
    echo "Warning: CONFIG_ALLWINNER_BINARY_ONLY is enabled."
    echo "You should usually run this script in SOURCE mode to capture the latest builds."
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Copy arch.a
if [ -f "${ARCH_LIB_SRC}" ]; then
    echo "Processing arch library..."
    mkdir -p "$(dirname "${ARCH_LIB_DST}")"
    cp -v "${ARCH_LIB_SRC}" "${ARCH_LIB_DST}"
else
    echo "Error: arch library not found at ${ARCH_LIB_SRC}"
    echo "Please run 'make' in nuttx first."
fi

# Copy board.a
if [ -f "${BOARD_LIB_SRC}" ]; then
    echo "Processing board library..."
    mkdir -p "$(dirname "${BOARD_LIB_DST}")"
    cp -v "${BOARD_LIB_SRC}" "${BOARD_LIB_DST}"
else
    echo "Error: board library not found at ${BOARD_LIB_SRC}"
    echo "Please run 'make' in nuttx first."
fi

# Create drivers library (r528)
echo "Processing drivers library (r528)..."
DRIVERS_OBJ_DIR="${TOPDIR}/drivers"
CANDIDATE_OBJS="realtek_netdev.o realtek_driver.o realtek_sdio.o realtek_gpio.o customer_rtos_service.o osdep_service.o wifi_conf.o wifi_ind.o wifi_promisc.o wifi_util.o device_lock.o net_stack_intf.o rtwlan_bsp.o wifi_io.o rtw_opt_crypto_ssl.o micro_sd_driver.o"
OBJ_FILES=""

if [ -d "${DRIVERS_OBJ_DIR}" ]; then
    mkdir -p "$(dirname "${DRIVER_LIB_DST}")"
    rm -f "${DRIVER_LIB_DST}"

    # Try to find AR tool
    if [ -z "$AR" ]; then
        AR="arm-none-eabi-ar"
        if ! command -v $AR &> /dev/null; then
             AR="ar"
        fi
    fi

    pushd "${DRIVERS_OBJ_DIR}" > /dev/null
    for obj in $CANDIDATE_OBJS; do
        if [ -f "$obj" ]; then
            OBJ_FILES="$OBJ_FILES $obj"
        else
            echo "Warning: Object $obj not found in ${DRIVERS_OBJ_DIR}"
        fi
    done

    if [ -n "$OBJ_FILES" ]; then
        echo "Creating ${DRIVER_LIB_DST} with objects: $OBJ_FILES"
        $AR cr "${DRIVER_LIB_DST}" $OBJ_FILES
    else
        echo "Error: No driver objects found to create library."
    fi
    popd > /dev/null
else
    echo "Error: Drivers object directory ${DRIVERS_OBJ_DIR} not found."
fi

echo "========================================"
echo "Done. Libraries are ready for binary-only distribution."
